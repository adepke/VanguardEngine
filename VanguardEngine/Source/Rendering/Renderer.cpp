// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/Renderer.h>
#include <Rendering/Resource.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Core/Config.h>

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

	device->GetResourceManager().Write(cameraBuffer, byteData);
}

void Renderer::CreatePipelines()
{
	PipelineStateDescription prepassStateDesc;

	prepassStateDesc.shaderPath = Config::shadersPath / "Prepass";

	prepassStateDesc.blendDescription.AlphaToCoverageEnable = false;
	prepassStateDesc.blendDescription.IndependentBlendEnable = false;
	prepassStateDesc.blendDescription.RenderTarget[0].BlendEnable = false;
	prepassStateDesc.blendDescription.RenderTarget[0].LogicOpEnable = false;
	prepassStateDesc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	prepassStateDesc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	prepassStateDesc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	prepassStateDesc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	prepassStateDesc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	prepassStateDesc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	prepassStateDesc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	prepassStateDesc.blendDescription.RenderTarget[0].RenderTargetWriteMask = 0;

	prepassStateDesc.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	prepassStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_NONE;//D3D12_CULL_MODE_BACK;  // #TEMP: Fix sponza model.
	prepassStateDesc.rasterizerDescription.FrontCounterClockwise = false;
	prepassStateDesc.rasterizerDescription.DepthBias = 0;
	prepassStateDesc.rasterizerDescription.DepthBiasClamp = 0.f;
	prepassStateDesc.rasterizerDescription.SlopeScaledDepthBias = 0.f;
	prepassStateDesc.rasterizerDescription.DepthClipEnable = true;
	prepassStateDesc.rasterizerDescription.MultisampleEnable = false;
	prepassStateDesc.rasterizerDescription.AntialiasedLineEnable = false;
	prepassStateDesc.rasterizerDescription.ForcedSampleCount = 0;
	prepassStateDesc.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	prepassStateDesc.depthStencilDescription.DepthEnable = true;  // Enable depth.
	prepassStateDesc.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;  // Write to the depth buffer.
	prepassStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_LESS;  // Traditional depth buffer.
	prepassStateDesc.depthStencilDescription.StencilEnable = false;
	prepassStateDesc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	prepassStateDesc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	prepassStateDesc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	prepassStateDesc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	prepassStateDesc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pipelines.AddGraphicsState(device.get(), "Prepass", prepassStateDesc);

	PipelineStateDescription forwardStateDesc;

	forwardStateDesc.shaderPath = Config::shadersPath / "Default";

	forwardStateDesc.blendDescription.AlphaToCoverageEnable = false;
	forwardStateDesc.blendDescription.IndependentBlendEnable = false;
	forwardStateDesc.blendDescription.RenderTarget[0].BlendEnable = false;
	forwardStateDesc.blendDescription.RenderTarget[0].LogicOpEnable = false;
	forwardStateDesc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	forwardStateDesc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	forwardStateDesc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	forwardStateDesc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	forwardStateDesc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	forwardStateDesc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	forwardStateDesc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	forwardStateDesc.blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	forwardStateDesc.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	forwardStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_NONE;//D3D12_CULL_MODE_BACK;  // #TEMP: Fix sponza model.
	forwardStateDesc.rasterizerDescription.FrontCounterClockwise = false;
	forwardStateDesc.rasterizerDescription.DepthBias = 0;
	forwardStateDesc.rasterizerDescription.DepthBiasClamp = 0.f;
	forwardStateDesc.rasterizerDescription.SlopeScaledDepthBias = 0.f;
	forwardStateDesc.rasterizerDescription.DepthClipEnable = true;
	forwardStateDesc.rasterizerDescription.MultisampleEnable = false;
	forwardStateDesc.rasterizerDescription.AntialiasedLineEnable = false;
	forwardStateDesc.rasterizerDescription.ForcedSampleCount = 0;
	forwardStateDesc.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// Prepass depth.
	forwardStateDesc.depthStencilDescription.DepthEnable = true;
	forwardStateDesc.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	forwardStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
	forwardStateDesc.depthStencilDescription.StencilEnable = false;
	forwardStateDesc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	forwardStateDesc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	forwardStateDesc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	forwardStateDesc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	forwardStateDesc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pipelines.AddGraphicsState(device.get(), "ForwardOpaque", forwardStateDesc);
}

std::pair<BufferHandle, size_t> Renderer::CreateInstanceBuffer(const entt::registry& registry)
{
	VGScopedCPUStat("Create Instance Buffer");

	const auto instanceView = registry.view<const TransformComponent, const MeshComponent>();
	const auto [bufferHandle, bufferOffset] = device->FrameAllocate(sizeof(EntityInstance) * instanceView.size());

	size_t index = 0;
	instanceView.each([&](auto entity, const auto& transform, const auto&)
	{
		const auto scaling = XMVectorSet(transform.scale.x, transform.scale.y, transform.scale.z, 0.f);
		const auto rotation = XMVectorSet(transform.rotation.x, transform.rotation.y, transform.rotation.z, 0.f);
		const auto translation = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);

		EntityInstance instance;
		instance.worldMatrix = XMMatrixAffineTransformation(scaling, XMVectorZero(), rotation, translation);

		std::vector<uint8_t> instanceData{};
		instanceData.resize(sizeof(EntityInstance));
		std::memcpy(instanceData.data(), &instance, instanceData.size());

		device->GetResourceManager().Write(bufferHandle, instanceData, bufferOffset + (index * sizeof(EntityInstance)));

		++index;
	});

	return std::make_pair(bufferHandle, bufferOffset);
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

	BufferDescription cameraBufferDesc{};
	cameraBufferDesc.updateRate = ResourceFrequency::Static;
	cameraBufferDesc.bindFlags = BindFlag::ConstantBuffer;
	cameraBufferDesc.accessFlags = AccessFlag::CPUWrite;
	cameraBufferDesc.size = 1;  // #TODO: Support multiple cameras.
	cameraBufferDesc.stride = sizeof(CameraBuffer);

	cameraBuffer = device->GetResourceManager().Create(cameraBufferDesc, VGText("Camera buffer"));

	userInterface = std::make_unique<UserInterfaceManager>(device.get());

	nullDescriptor = device->AllocateDescriptor(DescriptorType::Default);

	D3D12_SHADER_RESOURCE_VIEW_DESC nullViewDesc{};
	nullViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nullViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	nullViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	device->Native()->CreateShaderResourceView(nullptr, &nullViewDesc, nullDescriptor);

	CreatePipelines();
}

void Renderer::Render(entt::registry& registry)
{
	VGScopedCPUStat("Render");

	// Update the camera buffer immediately.
	UpdateCameraBuffer();

	RenderGraph graph{};
	
	const auto [instanceBuffer, instanceOffset] = CreateInstanceBuffer(registry);

	auto backBufferTag = graph.Import(device->GetBackBuffer());
	auto cameraBufferTag = graph.Import(cameraBuffer);

	graph.Tag(backBufferTag, ResourceTag::BackBuffer);
	
	auto& prePass = graph.AddPass("Prepass", ExecutionQueue::Graphics);
	auto depthStencilTag = prePass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R24G8_TYPELESS
	}, VGText("Depth stencil"));
	prePass.Read(cameraBufferTag, ResourceBind::CBV);
	prePass.Output(depthStencilTag, OutputBind::DSV, true);
	prePass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
	{
		auto cameraBuffer = resources.GetBuffer(cameraBufferTag);
		auto& cameraBufferComponent = device->GetResourceManager().Get(cameraBuffer);

		list.BindPipelineState(pipelines["Prepass"]);

		// Bind the camera data.
		list.Native()->SetGraphicsRootConstantBufferView(2, cameraBufferComponent.Native()->GetGPUVirtualAddress());

		size_t entityIndex = 0;
		registry.view<const TransformComponent, const MeshComponent>().each([&](auto entity, const auto&, const auto& mesh)
		{
			// Set the per object buffer.
			auto& instanceBufferComponent = device->GetResourceManager().Get(instanceBuffer);
			const auto finalInstanceBufferOffset = instanceOffset + (entityIndex * sizeof(EntityInstance));
			list.Native()->SetGraphicsRootConstantBufferView(0, instanceBufferComponent.Native()->GetGPUVirtualAddress() + finalInstanceBufferOffset);

			// Set the index buffer.
			auto& indexBuffer = device->GetResourceManager().Get(mesh.indexBuffer);

			D3D12_INDEX_BUFFER_VIEW indexView{};
			indexView.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress();
			indexView.SizeInBytes = static_cast<UINT>(indexBuffer.description.size * indexBuffer.description.stride);
			indexView.Format = DXGI_FORMAT_R32_UINT;

			list.Native()->IASetIndexBuffer(&indexView);

			for (const auto& subset : mesh.subsets)
			{
				// #TODO: Only bind once per mesh, and pass subset.vertexOffset into the draw call. This isn't yet supported with DXC, see: https://github.com/microsoft/DirectXShaderCompiler/issues/2907

				// Set the vertex buffer.
				auto& vertexBuffer = device->GetResourceManager().Get(mesh.vertexBuffer);
				list.Native()->SetGraphicsRootShaderResourceView(1, vertexBuffer.Native()->GetGPUVirtualAddress() + (subset.vertexOffset * sizeof(Vertex)));

				list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, static_cast<uint32_t>(subset.indexOffset), 0, 0);
			}

			++entityIndex;
		});
	});
	
	auto& forwardPass = graph.AddPass("Forward Pass", ExecutionQueue::Graphics);
	const auto mainOutputTag = forwardPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_B8G8R8A8_UNORM
		}, VGText("Main output"));
	forwardPass.Read(depthStencilTag, ResourceBind::DSV);
	forwardPass.Read(cameraBufferTag, ResourceBind::CBV);
	forwardPass.Output(mainOutputTag, OutputBind::RTV, true);
	forwardPass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
	{
		auto cameraBuffer = resources.GetBuffer(cameraBufferTag);
		auto& cameraBufferComponent = device->GetResourceManager().Get(cameraBuffer);

		list.BindPipelineState(pipelines["ForwardOpaque"]);

		// Set the bindless textures.
		list.Native()->SetGraphicsRootDescriptorTable(4, device->GetDescriptorAllocator().GetBindlessHeap());

		// Bind the camera data.
		list.Native()->SetGraphicsRootConstantBufferView(2, cameraBufferComponent.Native()->GetGPUVirtualAddress());

		size_t entityIndex = 0;
		registry.view<const TransformComponent, const MeshComponent>().each([&](auto entity, const auto&, const auto& mesh)
		{
			// Set the per object buffer.
			auto& instanceBufferComponent = device->GetResourceManager().Get(instanceBuffer);
			const auto finalInstanceBufferOffset = instanceOffset + (entityIndex * sizeof(EntityInstance));
			list.Native()->SetGraphicsRootConstantBufferView(0, instanceBufferComponent.Native()->GetGPUVirtualAddress() + finalInstanceBufferOffset);

			// Set the index buffer.
			auto& indexBuffer = device->GetResourceManager().Get(mesh.indexBuffer);

			D3D12_INDEX_BUFFER_VIEW indexView{};
			indexView.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress();
			indexView.SizeInBytes = static_cast<UINT>(indexBuffer.description.size * indexBuffer.description.stride);
			indexView.Format = DXGI_FORMAT_R32_UINT;

			list.Native()->IASetIndexBuffer(&indexView);

			for (const auto& subset : mesh.subsets)
			{
				// #TODO: Only bind once per mesh, and pass subset.vertexOffset into the draw call. This isn't yet supported with DXC, see: https://github.com/microsoft/DirectXShaderCompiler/issues/2907

				// Set the vertex buffer.
				auto& vertexBuffer = device->GetResourceManager().Get(mesh.vertexBuffer);
				list.Native()->SetGraphicsRootShaderResourceView(1, vertexBuffer.Native()->GetGPUVirtualAddress() + (subset.vertexOffset * sizeof(Vertex)));

				// Set the material data.
				if (device->GetResourceManager().Valid(subset.material.materialBuffer))
				{
					auto& materialBuffer = device->GetResourceManager().Get(subset.material.materialBuffer);
					list.Native()->SetGraphicsRootConstantBufferView(3, materialBuffer.Native()->GetGPUVirtualAddress());
				}

				list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, static_cast<uint32_t>(subset.indexOffset), 0, 0);
			}

			++entityIndex;
		});
	});

#if ENABLE_EDITOR
	auto& editorPass = graph.AddPass("Editor Pass", ExecutionQueue::Graphics);
	editorPass.Read(depthStencilTag, ResourceBind::SRV);
	editorPass.Read(mainOutputTag, ResourceBind::SRV);
	editorPass.Output(backBufferTag, OutputBind::RTV, false);
	editorPass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
	{
		userInterface->NewFrame();
		EditorRenderer::Render(device.get(), registry, lastFrameTime, resources.GetTexture(depthStencilTag), resources.GetTexture(mainOutputTag));
		userInterface->Render(list);
	});
#endif

	graph.Build();
	graph.Execute(device.get());

	// #TODO: Move to a present pass.
	device->GetSwapChain()->Present(device->vSync, 0);

	device->AdvanceGPU();
}