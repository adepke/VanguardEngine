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

// Per-entity data.
struct EntityInstance
{
	// #TODO: Create from some form of shader interop.

	XMMATRIX WorldMatrix;
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

	// #TODO: Culling.

	// #TODO: Sort by material.
	
	// #TEMP: Number of entities with MeshComponent and TransformComponent. Determine this procedurally.
	const auto RenderCount = 1;

	std::pair<std::shared_ptr<Buffer>, size_t> InstanceBuffer;

	{
		VGScopedCPUStat("Generate Instance Buffer");

		InstanceBuffer = std::move(Device->FrameAllocate(sizeof(EntityInstance) * RenderCount));

		size_t Index = 0;
		Registry.view<const TransformComponent, const MeshComponent>().each([this, &InstanceBuffer, &Index](auto Entity, const auto& Transform, const auto&)
			{
				const auto Scaling = XMVectorSet(Transform.Scale.x, Transform.Scale.y, Transform.Scale.z, 0.f);
				const auto Rotation = XMVectorSet(Transform.Rotation.x, Transform.Rotation.y, Transform.Rotation.z, 0.f);
				const auto Translation = XMVectorSet(Transform.Translation.x, Transform.Translation.y, Transform.Translation.z, 0.f);

				EntityInstance Instance;
				Instance.WorldMatrix = XMMatrixAffineTransformation(Scaling, XMVectorZero(), Rotation, Translation);

				std::vector<uint8_t> InstanceData{};
				InstanceData.resize(sizeof(EntityInstance));
				std::memcpy(InstanceData.data(), &Instance, InstanceData.size());

				Device->WriteResource(InstanceBuffer.first, InstanceData, InstanceBuffer.second + (Index * sizeof(EntityInstance)));

				++Index;
			});
	}

	RenderGraph Graph{ Device.get() };

	const size_t BackBufferTag = Graph.ImportResource(Device->GetBackBuffer());
	const size_t CameraBufferTag = Graph.ImportResource(cameraBuffer);
	const size_t InstanceBufferTag = Graph.ImportResource(InstanceBuffer.first);

	RGTextureDescription DepthStencilDesc{};
	DepthStencilDesc.Width = Device->RenderWidth;
	DepthStencilDesc.Height = Device->RenderHeight;
	DepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	auto& MainPass = Graph.AddPass("Main Pass");
	const size_t DepthStencilTag = MainPass.CreateResource(DepthStencilDesc, VGText("Depth Stencil"));
	MainPass.WriteResource(DepthStencilTag, RGUsage::DepthStencil);
	MainPass.WriteResource(BackBufferTag, RGUsage::BackBuffer);
	MainPass.ReadResource(CameraBufferTag, RGUsage::Default);
	MainPass.ReadResource(InstanceBufferTag, RGUsage::Default);

	MainPass.Bind(
		[this, &Registry, BackBufferTag, DepthStencilTag, CameraBufferTag, InstanceBufferTag, InstanceBufferOffset = InstanceBuffer.second]
		(RGResolver& Resolver, CommandList& List)
		{
			auto& BackBuffer = Resolver.Get<Texture>(BackBufferTag);
			auto& DepthStencil = Resolver.Get<Texture>(DepthStencilTag);
			auto& ActualCameraBuffer = Resolver.Get<Buffer>(CameraBufferTag);  // #TODO: Fix naming.
			auto& InstanceBuffer = Resolver.Get<Buffer>(InstanceBufferTag);

			List.BindPipelineState(*Materials[0].Pipeline);
			List.BindDescriptorAllocator(Device->GetDescriptorAllocator());
			
			List.Native()->SetGraphicsRootConstantBufferView(2, ActualCameraBuffer.Native()->GetGPUVirtualAddress());

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
			
			size_t EntityIndex = 0;
			Registry.view<const TransformComponent, MeshComponent>().each(
				[&EntityIndex, &List, &InstanceBuffer, InstanceBufferOffset](auto Entity, const auto&, auto& Mesh)
				{
					// Set the per object buffer.
					const auto FinalInstanceBufferOffset = InstanceBufferOffset + (EntityIndex * sizeof(EntityInstance));
					List.Native()->SetGraphicsRootConstantBufferView(0, InstanceBuffer.Native()->GetGPUVirtualAddress() + FinalInstanceBufferOffset);

					// Set the vertex buffer.
					List.Native()->SetGraphicsRootShaderResourceView(1, Mesh.VertexBuffer->Native()->GetGPUVirtualAddress());

					D3D12_INDEX_BUFFER_VIEW IndexView{};
					IndexView.BufferLocation = Mesh.IndexBuffer->Native()->GetGPUVirtualAddress();
					IndexView.SizeInBytes = static_cast<UINT>(Mesh.IndexBuffer->Description.Size * Mesh.IndexBuffer->Description.Stride);
					IndexView.Format = DXGI_FORMAT_R32_UINT;

					List.Native()->IASetIndexBuffer(&IndexView);

					List.Native()->DrawIndexedInstanced(Mesh.IndexBuffer->Description.Size, 1, 0, 0, 0);

					++EntityIndex;
				});
		}
	);

	auto& UIPass = Graph.AddPass("UI Pass");
	UIPass.WriteResource(BackBufferTag, RGUsage::BackBuffer);

	UIPass.Bind(
		[this, BackBufferTag, &Registry](RGResolver& Resolver, CommandList& List)
		{
			auto& BackBuffer = Resolver.Get<Texture>(BackBufferTag);

			UserInterface->NewFrame();

			// #TEMP: Temporary camera settings utility for debugging.

			float CameraPosX, CameraPosY, CameraPosZ, FOV;

			Registry.view<TransformComponent, CameraComponent>().each(
				[&CameraPosX, &CameraPosY, &CameraPosZ, &FOV](auto Entity, auto& Transform, auto& Camera)
				{
					CameraPosX = Transform.Translation.x;
					CameraPosY = Transform.Translation.y;
					CameraPosZ = Transform.Translation.z;
					FOV = Camera.FieldOfView * 180.f / 3.14159f;
				});
			
			ImGui::Begin("Camera Settings");
			ImGui::SliderFloat("X", &CameraPosX, -20.f, 20.f);
			ImGui::SliderFloat("Y", &CameraPosY, -20.f, 20.f);
			ImGui::SliderFloat("Z", &CameraPosZ, -20.f, 20.f);
			ImGui::SliderFloat("FOV", &FOV, 45.f, 120.f);
			ImGui::End();

			Registry.view<TransformComponent, CameraComponent>().each(
				[CameraPosX, CameraPosY, CameraPosZ, FOV](auto Entity, auto& Transform, auto& Camera)
				{
					Transform.Translation.x = CameraPosX;
					Transform.Translation.y = CameraPosY;
					Transform.Translation.z = CameraPosZ;
					Camera.FieldOfView = FOV * 3.14159f / 180.f;
				});

			// #TEMP: Entity viewer, move this into the editor module.

			const auto EntityView = Registry.view<TransformComponent>();
			const auto EntityCount = EntityView.size();
			
			ImGui::Begin("Entity Viewer");
			ImGui::Text("%i Entities", EntityCount);
			EntityView.each([](auto Entity, auto& Transform)
				{
					if (ImGui::TreeNode((void*)Entity, "ID: %i", Entity))
					{
						ImGui::InputFloat("X", &Transform.Translation.x);
						ImGui::InputFloat("Y", &Transform.Translation.y);
						ImGui::InputFloat("Z", &Transform.Translation.z);

						ImGui::TreePop();
					}
				});
			ImGui::End();
			
			List.Native()->OMSetRenderTargets(1, &static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(*BackBuffer.RTV), false, nullptr);
			UserInterface->Render(List);
		});
		
	Graph.Build();

	Graph.Execute();

	{
		VGScopedCPUStat("Present");

		Device->GetSwapChain()->Present(Device->VSync, 0);
	}

	Device->AdvanceGPU();
}