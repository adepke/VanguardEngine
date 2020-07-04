// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/Renderer.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/MaterialManager.h>

#if ENABLE_EDITOR
#include <Editor/EditorRenderer.h>
#endif

#include <imgui.h>

#include <vector>
#include <utility>
#include <cstring>

// Per-entity data.
struct EntityInstance
{
	// #TODO: Create from some form of shader interop.

	XMMATRIX worldMatrix;
};

struct CameraBuffer
{
	XMMATRIX viewMatrix;
	XMMATRIX projectionMatrix;
};

void Renderer::UpdateCameraBuffer()
{
	VGScopedCPUStat("Update Camera Buffer");

	CameraBuffer bufferData;
	bufferData.viewMatrix = globalViewMatrix;
	bufferData.projectionMatrix = globalProjectionMatrix;

	std::vector<uint8_t> byteData;
	byteData.resize(sizeof(CameraBuffer));
	std::memcpy(byteData.data(), &bufferData, sizeof(bufferData));

	device->WriteResource(cameraBuffer, byteData);
}

Renderer::~Renderer()
{
	// Sync the device so that resource members in Renderer.h don't get destroyed while in-flight.
	device->SyncInterframe(true);
}

void Renderer::Initialize(std::unique_ptr<WindowFrame>&& inWindow, std::unique_ptr<RenderDevice>&& inDevice)
{
	VGScopedCPUStat("Renderer Initialize");

	window = std::move(inWindow);
	device = std::move(inDevice);

	device->CheckFeatureSupport();
	materials = std::move(MaterialManager::Get().ReloadMaterials(*device));

	BufferDescription cameraBufferDesc{};
	cameraBufferDesc.updateRate = ResourceFrequency::Static;
	cameraBufferDesc.bindFlags = BindFlag::ConstantBuffer;
	cameraBufferDesc.accessFlags = AccessFlag::CPUWrite;
	cameraBufferDesc.size = 1;  // #TODO: Support multiple cameras.
	cameraBufferDesc.stride = sizeof(CameraBuffer);

	cameraBuffer = device->CreateResource(cameraBufferDesc, VGText("Camera Buffer"));

	userInterface = std::make_unique<UserInterfaceManager>(device.get());

	nullDescriptor = device->AllocateDescriptor(DescriptorType::Default);

	D3D12_SHADER_RESOURCE_VIEW_DESC nullViewDesc{};
	nullViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nullViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	nullViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	device->Native()->CreateShaderResourceView(nullptr, &nullViewDesc, nullDescriptor);
}

void Renderer::Render(entt::registry& registry)
{
	VGScopedCPUStat("Render");

	// Update the camera buffer immediately.
	UpdateCameraBuffer();

	// #TODO: Culling.

	// #TODO: Sort by material.

	std::pair<std::shared_ptr<Buffer>, size_t> instanceBuffer;

	{
		VGScopedCPUStat("Generate Instance Buffer");

		const auto instanceView = registry.view<const TransformComponent, const MeshComponent>();

		instanceBuffer = std::move(device->FrameAllocate(sizeof(EntityInstance) * instanceView.size()));

		size_t index = 0;
		instanceView.each([this, &instanceBuffer, &index](auto entity, const auto& transform, const auto&)
			{
				const auto scaling = XMVectorSet(transform.scale.x, transform.scale.y, transform.scale.z, 0.f);
				const auto rotation = XMVectorSet(transform.rotation.x, transform.rotation.y, transform.rotation.z, 0.f);
				const auto translation = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);

				EntityInstance instance;
				instance.worldMatrix = XMMatrixAffineTransformation(scaling, XMVectorZero(), rotation, translation);

				std::vector<uint8_t> instanceData{};
				instanceData.resize(sizeof(EntityInstance));
				std::memcpy(instanceData.data(), &instance, instanceData.size());

				device->WriteResource(instanceBuffer.first, instanceData, instanceBuffer.second + (index * sizeof(EntityInstance)));

				++index;
			});
	}

	RenderGraph graph{ device.get() };

	const size_t backBufferTag = graph.ImportResource(device->GetBackBuffer());
	const size_t cameraBufferTag = graph.ImportResource(cameraBuffer);
	const size_t instanceBufferTag = graph.ImportResource(instanceBuffer.first);

	RGTextureDescription depthStencilDesc{};
	depthStencilDesc.width = device->renderWidth;
	depthStencilDesc.height = device->renderHeight;
	depthStencilDesc.format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	auto& mainPass = graph.AddPass("Main Pass");
	const size_t depthStencilTag = mainPass.CreateResource(depthStencilDesc, VGText("Depth Stencil"));
	mainPass.WriteResource(depthStencilTag, RGUsage::DepthStencil);
	mainPass.WriteResource(backBufferTag, RGUsage::BackBuffer);
	mainPass.ReadResource(cameraBufferTag, RGUsage::Default);
	mainPass.ReadResource(instanceBufferTag, RGUsage::Default);

	mainPass.Bind(
		[this, &registry, backBufferTag, depthStencilTag, cameraBufferTag, instanceBufferTag, instanceBufferOffset = instanceBuffer.second]
	(RGResolver& resolver, CommandList& list)
	{
		auto& backBuffer = resolver.Get<Texture>(backBufferTag);
		auto& depthStencil = resolver.Get<Texture>(depthStencilTag);
		auto& cameraBuffer = resolver.Get<Buffer>(cameraBufferTag);
		auto& instanceBuffer = resolver.Get<Buffer>(instanceBufferTag);

		list.BindPipelineState(*materials[0].pipeline);
		list.BindDescriptorAllocator(device->GetDescriptorAllocator());

		list.Native()->SetGraphicsRootConstantBufferView(2, cameraBuffer.Native()->GetGPUVirtualAddress());

		D3D12_VIEWPORT viewport{};

#if ENABLE_EDITOR
		viewport = EditorRenderer::GetSceneViewport();
#else
		viewport.TopLeftX = 0.f;
		viewport.TopLeftY = 0.f;
		viewport.Width = device->renderWidth;
		viewport.Height = device->renderHeight;
		viewport.MinDepth = 0.f;
		viewport.MaxDepth = 1.f;
#endif

		D3D12_RECT scissorRect{};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = device->renderWidth;
		scissorRect.bottom = device->renderHeight;

		list.Native()->RSSetViewports(1, &viewport);
		list.Native()->RSSetScissorRects(1, &scissorRect);
		list.Native()->OMSetRenderTargets(1, &static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(*backBuffer.RTV), false, &static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(*depthStencil.DSV));
		list.Native()->OMSetStencilRef(0);
		const float ClearColor[] = { 0.2f, 0.2f, 0.2f, 1.f };
		list.Native()->ClearRenderTargetView(*backBuffer.RTV, ClearColor, 0, nullptr);
		list.Native()->ClearDepthStencilView(*depthStencil.DSV, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

		size_t entityIndex = 0;
		registry.view<const TransformComponent, MeshComponent>().each(
			[this, &entityIndex, &list, &instanceBuffer, instanceBufferOffset](auto entity, const auto&, auto& mesh)
			{
				// Set the per object buffer.
				const auto finalInstanceBufferOffset = instanceBufferOffset + (entityIndex * sizeof(EntityInstance));
				list.Native()->SetGraphicsRootConstantBufferView(0, instanceBuffer.Native()->GetGPUVirtualAddress() + finalInstanceBufferOffset);

				// Set the index buffer.
				D3D12_INDEX_BUFFER_VIEW indexView{};
				indexView.BufferLocation = mesh.indexBuffer->Native()->GetGPUVirtualAddress();
				indexView.SizeInBytes = static_cast<UINT>(mesh.indexBuffer->description.size * mesh.indexBuffer->description.stride);
				indexView.Format = DXGI_FORMAT_R32_UINT;

				list.Native()->IASetIndexBuffer(&indexView);

				for (const auto& subset : mesh.subsets)
				{
					// #TODO: Only bind once per mesh, and pass Subset.VertexOffset into the draw call. This isn't yet supported with DXC, see: https://github.com/microsoft/DirectXShaderCompiler/issues/2907

					// Set the vertex buffer.
					list.Native()->SetGraphicsRootShaderResourceView(1, mesh.vertexBuffer->Native()->GetGPUVirtualAddress() + (subset.vertexOffset * sizeof(Vertex)));

					if (subset.material)
					{
						auto& albedoSRV = subset.material->albedo ? *subset.material->albedo->SRV : nullDescriptor;
						auto& normalSRV = subset.material->normal ? *subset.material->normal->SRV : nullDescriptor;
						auto& roughnessSRV = subset.material->roughness ? *subset.material->roughness->SRV : nullDescriptor;
						auto& metallicSRV = subset.material->metallic ? *subset.material->metallic->SRV : nullDescriptor;

						device->GetDescriptorAllocator().AddTableEntry(albedoSRV, DescriptorTableEntryType::ShaderResource);
						device->GetDescriptorAllocator().AddTableEntry(normalSRV, DescriptorTableEntryType::ShaderResource);
						device->GetDescriptorAllocator().AddTableEntry(roughnessSRV, DescriptorTableEntryType::ShaderResource);
						device->GetDescriptorAllocator().AddTableEntry(metallicSRV, DescriptorTableEntryType::ShaderResource);

						device->GetDescriptorAllocator().BuildTable(*device, list, 3);
					}

					list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, static_cast<uint32_t>(subset.indexOffset), 0, 0);
				}

				++entityIndex;
			});
	}
	);

	auto& uiPass = graph.AddPass("UI Pass");
	uiPass.WriteResource(backBufferTag, RGUsage::BackBuffer);

	uiPass.Bind(
		[this, backBufferTag, &registry](RGResolver& resolver, CommandList& list)
		{
			auto& backBuffer = resolver.Get<Texture>(backBufferTag);

			userInterface->NewFrame();

#if ENABLE_EDITOR
			EditorRenderer::Render(registry);
#endif

			list.Native()->OMSetRenderTargets(1, &static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(*backBuffer.RTV), false, nullptr);
			userInterface->Render(list);
		});

	graph.Build();

	graph.Execute();

	{
		VGScopedCPUStat("Present");

		device->GetSwapChain()->Present(device->vSync, 0);
	}

	device->AdvanceGPU();
}