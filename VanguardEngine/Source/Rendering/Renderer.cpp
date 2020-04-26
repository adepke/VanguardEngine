// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/Renderer.h>
#include <Rendering/Device.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderGraph.h>

//#include <entt/entt.hpp>  // #TODO: Include from here instead of in the header.
#include <imgui.h>

#include <vector>
#include <utility>
#include <cstring>

// Per-entity data, passed through input assembler.
struct EntityInstance
{
	// #TODO: Create from some form of shader interop.

	TransformComponent Transform;
};

struct CameraBuffer
{
	XMMATRIX ViewMatrix;
	XMMATRIX ProjectionMatrix;
};

void Renderer::UpdateCameraBuffer()
{
	CameraBuffer BufferData;
	BufferData.ViewMatrix = GlobalViewMatrix;
	BufferData.ProjectionMatrix = GlobalProjectionMatrix;

	std::vector<uint8_t> ByteData;
	ByteData.resize(sizeof(CameraBuffer));
	std::memcpy(ByteData.data(), &BufferData, sizeof(BufferData));

	Device->WriteResource(cameraBuffer, ByteData);
}

void Renderer::Initialize(std::unique_ptr<RenderDevice>&& InDevice)
{
	VGScopedCPUStat("Renderer Initialize");

	Device = std::move(InDevice);

	Device->CheckFeatureSupport();
	Materials = std::move(Device->ReloadMaterials());

	BufferDescription CameraBufferDesc{};
	CameraBufferDesc.UpdateRate = ResourceFrequency::Static;
	CameraBufferDesc.BindFlags = BindFlag::ConstantBuffer;
	CameraBufferDesc.AccessFlags = AccessFlag::CPUWrite;
	CameraBufferDesc.Size = 1;  // #TODO: Support multiple cameras.
	CameraBufferDesc.Stride = sizeof(CameraBuffer);

	cameraBuffer = Device->CreateResource(CameraBufferDesc, VGText("Camera Buffer"));

	UserInterface = std::make_unique<UserInterfaceManager>(Device.get());
}

void Renderer::Render(entt::registry& Registry)
{
	VGScopedCPUStat("Render");

	// Update the camera buffer immediately.
	UpdateCameraBuffer();

	Device->GetCopyList().Close();
	ID3D12CommandList* CopyLists[] = { Device->GetCopyList().Native() };

	{
		VGScopedCPUStat("Execute Copy");

		// This will decay our resources back to common.
		Device->GetCopyQueue()->ExecuteCommandLists(1, CopyLists);
	}

	// #TODO: Culling.

	// #TODO: Sort by material.
	
	// Number of entities with MeshComponent and TransformComponent
	const auto RenderCount = 10;

	std::pair<std::shared_ptr<Buffer>, size_t> InstanceBuffer;

	{
		VGScopedCPUStat("Generate Instance Buffer");

		InstanceBuffer = std::move(Device->FrameAllocate(sizeof(EntityInstance) * RenderCount));

		size_t Index = 0;
		Registry.view<const TransformComponent, const MeshComponent>().each([this, &InstanceBuffer, &Index](auto Entity, const auto& Transform, const auto&)
			{
				EntityInstance Instance{ Transform };

				std::vector<uint8_t> InstanceData{};
				InstanceData.resize(sizeof(EntityInstance));
				std::memcpy(InstanceData.data(), &Instance, InstanceData.size());

				Device->WriteResource(InstanceBuffer.first, InstanceData, InstanceBuffer.second + (Index * sizeof(EntityInstance)));

				++Index;
			});
	}

	RenderGraph Graph{ Device.get() };

	const size_t BackBufferTag = Graph.ImportResource(Device->GetBackBuffer());

	RGTextureDescription DepthStencilDesc{};
	DepthStencilDesc.Width = Device->RenderWidth;
	DepthStencilDesc.Height = Device->RenderHeight;
	DepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	auto& MainPass = Graph.AddPass("Main Pass");
	const size_t DepthStencilTag = MainPass.CreateResource(DepthStencilDesc, VGText("Depth Stencil"));
	MainPass.WriteResource(DepthStencilTag, RGUsage::DepthStencil);
	MainPass.WriteResource(BackBufferTag, RGUsage::BackBuffer);

	MainPass.Bind(
		[this, &Registry, &InstanceBuffer, BackBufferTag, DepthStencilTag](RGResolver& Resolver, CommandList& List)
		{
			auto& BackBuffer = Resolver.Get<Texture>(BackBufferTag);
			auto& DepthStencil = Resolver.Get<Texture>(DepthStencilTag);

			List.BindPipelineState(*Materials[0].Pipeline);
			List.BindDescriptorAllocator(Device->GetDescriptorAllocator());
			
			List.Native()->SetGraphicsRootConstantBufferView(0, cameraBuffer->Native()->GetGPUVirtualAddress());

			D3D12_VIEWPORT Viewport{};
			Viewport.TopLeftX = 0.f;
			Viewport.TopLeftY = 0.f;
			Viewport.Width = Device->RenderWidth;
			Viewport.Height = Device->RenderHeight;
			Viewport.MinDepth = 0.f;
			Viewport.MaxDepth = 1.f;

			D3D12_RECT ScissorRect{};
			ScissorRect.left = 0;
			ScissorRect.top = 0;
			ScissorRect.right = Device->RenderWidth;
			ScissorRect.bottom = Device->RenderHeight;

			List.Native()->RSSetViewports(1, &Viewport);
			List.Native()->RSSetScissorRects(1, &ScissorRect);
			List.Native()->OMSetRenderTargets(1, &static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(*BackBuffer.RTV), false, &static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(*DepthStencil.DSV));
			List.Native()->OMSetStencilRef(0);
			const float ClearColor[] = { 0.2f, 0.2f, 0.2f, 1.f };
			List.Native()->ClearRenderTargetView(*BackBuffer.RTV, ClearColor, 0, nullptr);
			List.Native()->ClearDepthStencilView(*DepthStencil.DSV, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
			
			Registry.view<const TransformComponent, MeshComponent>().each([&InstanceBuffer, &List](auto Entity, const auto&, auto& Mesh)
				{
					std::vector<D3D12_VERTEX_BUFFER_VIEW> VertexViews;

					// Vertex Buffer.
					VertexViews.push_back({});
					VertexViews[0].BufferLocation = Mesh.VertexBuffer->Native()->GetGPUVirtualAddress();
					VertexViews[0].SizeInBytes = static_cast<UINT>(Mesh.VertexBuffer->Description.Size * Mesh.VertexBuffer->Description.Stride);
					VertexViews[0].StrideInBytes = static_cast<UINT>(Mesh.VertexBuffer->Description.Stride);

					/*
					// Instance Buffer.
					VertexViews.push_back({});
					VertexViews[1].BufferLocation = InstanceBuffer.first->Native()->GetGPUVirtualAddress() + InstanceBuffer.second;
					VertexViews[1].SizeInBytes = static_cast<UINT>(InstanceBuffer.first->Description.Size);
					VertexViews[1].StrideInBytes = static_cast<UINT>(InstanceBuffer.first->Description.Stride);
					*/

					List.Native()->IASetVertexBuffers(0, static_cast<UINT>(VertexViews.size()), VertexViews.data());  // #TODO: Slot?

					D3D12_INDEX_BUFFER_VIEW IndexView{};
					IndexView.BufferLocation = Mesh.IndexBuffer->Native()->GetGPUVirtualAddress();
					IndexView.SizeInBytes = static_cast<UINT>(Mesh.IndexBuffer->Description.Size * Mesh.IndexBuffer->Description.Stride);
					IndexView.Format = DXGI_FORMAT_R32_UINT;

					List.Native()->IASetIndexBuffer(&IndexView);

					List.Native()->DrawIndexedInstanced(Mesh.IndexBuffer->Description.Size, 1, 0, 0, 0);
				});
		}
	);
	
	auto& UIPass = Graph.AddPass("UI Pass");
	UIPass.WriteResource(BackBufferTag, RGUsage::BackBuffer);

	UIPass.Bind(
		[this, BackBufferTag](RGResolver& Resolver, CommandList& List)
		{
			auto& BackBuffer = Resolver.Get<Texture>(BackBufferTag);

			UserInterface->NewFrame();
			
			ImGui::ShowDemoWindow();

			List.Native()->OMSetRenderTargets(1, &static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(*BackBuffer.RTV), false, nullptr);
			for (int i = 0; i < 50; ++i)
				UserInterface->Render(List);
		});

	Graph.Build();

	Graph.Execute();

	{
		VGScopedCPUStat("Present");

		Device->GetSwapChain()->Present(Device->VSync, 0);  // #TODO: This is probably presenting the wrong frame!
	}

	Device->AdvanceGPU();
}