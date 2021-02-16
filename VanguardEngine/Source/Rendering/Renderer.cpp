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
	XMFLOAT3 position;
	uint32_t padding;
};

struct Light
{
	XMFLOAT3 position;
	uint32_t type;
	XMFLOAT3 color;
	uint32_t padding;
};

void Renderer::UpdateCameraBuffer(const entt::registry& registry)
{
	VGScopedCPUStat("Update Camera Buffer");

	XMFLOAT3 translation;
	registry.view<const TransformComponent, const CameraComponent>().each([&](auto entity, const auto& transform, const auto&)
	{
		// #TODO: Support more than one camera.
		translation = transform.translation;
	});

	CameraBuffer bufferData;
	bufferData.viewMatrix = globalViewMatrix;
	bufferData.projectionMatrix = globalProjectionMatrix;
	bufferData.position = translation;

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
	prepassStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_BACK;
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
	prepassStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;  // Inverse depth buffer.
	prepassStateDesc.depthStencilDescription.StencilEnable = false;
	prepassStateDesc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	prepassStateDesc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	prepassStateDesc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	prepassStateDesc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	prepassStateDesc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pipelines.AddGraphicsState(device.get(), "Prepass", prepassStateDesc, false);

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
	forwardStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_BACK;
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

	pipelines.AddGraphicsState(device.get(), "ForwardOpaque", forwardStateDesc, false);

	forwardStateDesc.shaderPath = Config::shadersPath / "Default";

	// Disable back face culling.
	forwardStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_NONE;

	// Transparency blending.
	forwardStateDesc.blendDescription.AlphaToCoverageEnable = false;  // Not using MSAA.
	forwardStateDesc.blendDescription.IndependentBlendEnable = false;  // Only rendering to a single render target, discard others.
	forwardStateDesc.blendDescription.RenderTarget[0].BlendEnable = true;
	forwardStateDesc.blendDescription.RenderTarget[0].LogicOpEnable = false;  // No special blend logic.
	forwardStateDesc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	forwardStateDesc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	forwardStateDesc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	forwardStateDesc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	forwardStateDesc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	forwardStateDesc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	forwardStateDesc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	forwardStateDesc.blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// Test against the prepass depth, but don't use EQUAL operation since we're not writing transparent object data
	// into the buffer, so it would result in transparent pixels only being drawn when overlapping with an opaque pixel.
	forwardStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	pipelines.AddGraphicsState(device.get(), "ForwardTransparent", forwardStateDesc, false);

	PipelineStateDescription postProcessStateDesc;

	postProcessStateDesc.shaderPath = Config::shadersPath / "PostProcess";

	postProcessStateDesc.blendDescription.AlphaToCoverageEnable = false;
	postProcessStateDesc.blendDescription.IndependentBlendEnable = false;
	postProcessStateDesc.blendDescription.RenderTarget[0].BlendEnable = false;
	postProcessStateDesc.blendDescription.RenderTarget[0].LogicOpEnable = false;
	postProcessStateDesc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	postProcessStateDesc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	postProcessStateDesc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	postProcessStateDesc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	postProcessStateDesc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	postProcessStateDesc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	postProcessStateDesc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	postProcessStateDesc.blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	postProcessStateDesc.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	postProcessStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_BACK;
	postProcessStateDesc.rasterizerDescription.FrontCounterClockwise = false;
	postProcessStateDesc.rasterizerDescription.DepthBias = 0;
	postProcessStateDesc.rasterizerDescription.DepthBiasClamp = 0.f;
	postProcessStateDesc.rasterizerDescription.SlopeScaledDepthBias = 0.f;
	postProcessStateDesc.rasterizerDescription.DepthClipEnable = true;
	postProcessStateDesc.rasterizerDescription.MultisampleEnable = false;
	postProcessStateDesc.rasterizerDescription.AntialiasedLineEnable = false;
	postProcessStateDesc.rasterizerDescription.ForcedSampleCount = 0;
	postProcessStateDesc.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	postProcessStateDesc.depthStencilDescription.DepthEnable = false;
	postProcessStateDesc.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	postProcessStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_NEVER;
	postProcessStateDesc.depthStencilDescription.StencilEnable = false;
	postProcessStateDesc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	postProcessStateDesc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	postProcessStateDesc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	postProcessStateDesc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	postProcessStateDesc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pipelines.AddGraphicsState(device.get(), "PostProcess", postProcessStateDesc, true);
}

std::pair<BufferHandle, size_t> Renderer::CreateInstanceBuffer(const entt::registry& registry)
{
	VGScopedCPUStat("Create Instance Buffer");

	const auto instanceView = registry.view<const TransformComponent, const MeshComponent>();
	const auto viewSize = instanceView.size();
	const auto [bufferHandle, bufferOffset] = device->FrameAllocate(sizeof(EntityInstance) * viewSize);

	size_t index = 0;
	instanceView.each([&](auto entity, const auto& transform, const auto&)
	{
		const auto scaling = XMVectorSet(transform.scale.x, transform.scale.y, transform.scale.z, 0.f);
		const auto rotation = XMVectorSet(transform.rotation.x, transform.rotation.y, transform.rotation.z, 0.f);
		const auto translation = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);

		const auto scalingMat = XMMatrixScalingFromVector(scaling);
		const auto rotationMat = XMMatrixRotationX(-transform.rotation.x) * XMMatrixRotationY(-transform.rotation.y) * XMMatrixRotationZ(-transform.rotation.z);
		const auto translationMat = XMMatrixTranslationFromVector(translation);

		EntityInstance instance;
		instance.worldMatrix = scalingMat * rotationMat * translationMat;

		std::vector<uint8_t> instanceData{};
		instanceData.resize(sizeof(EntityInstance));
		std::memcpy(instanceData.data(), &instance, instanceData.size());

		device->GetResourceManager().Write(bufferHandle, instanceData, bufferOffset + (index * sizeof(EntityInstance)));

		++index;
	});

	VGAssert(viewSize == index, "Mismatched entity count during buffer creation.");

	return std::make_pair(bufferHandle, bufferOffset);
}

BufferHandle Renderer::CreateLightBuffer(const entt::registry& registry)
{
	VGScopedCPUStat("Create Light Buffer");

	const auto lightView = registry.view<const TransformComponent, const LightComponent>();
	const auto viewSize = lightView.size();

	BufferDescription lightBufferDescription;
	lightBufferDescription.updateRate = ResourceFrequency::Dynamic;
	lightBufferDescription.bindFlags = BindFlag::ShaderResource;
	lightBufferDescription.accessFlags = AccessFlag::CPUWrite;
	lightBufferDescription.size = viewSize;
	lightBufferDescription.stride = sizeof(Light);

	const auto bufferHandle = device->GetResourceManager().Create(lightBufferDescription, VGText("Light buffer"));
	device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), bufferHandle);

	size_t index = 0;
	lightView.each([&](auto entity, const auto& transform, const auto& light)
	{
		Light instance{
			.position = transform.translation,
			.type = 0,
			.color = light.color
		};

		std::vector<uint8_t> instanceData{};
		instanceData.resize(sizeof(Light));
		std::memcpy(instanceData.data(), &instance, instanceData.size());

		device->GetResourceManager().Write(bufferHandle, instanceData, index * sizeof(Light));

		++index;
	});

	VGAssert(viewSize == index, "Mismatched entity count during buffer creation.");

	return bufferHandle;
}

Renderer::~Renderer()
{
	// Sync the device so that resource members in Renderer.h don't get destroyed while in-flight.
	device->Synchronize();
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
	UpdateCameraBuffer(registry);

	RenderGraph graph{};
	
	const auto [instanceBuffer, instanceOffset] = CreateInstanceBuffer(registry);
	const auto lightBuffer = CreateLightBuffer(registry);

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
		list.BindPipelineState(pipelines["Prepass"]);
		list.BindResource("cameraBuffer", resources.GetBuffer(cameraBufferTag));

		size_t entityIndex = 0;
		registry.view<const TransformComponent, const MeshComponent>().each([&](auto entity, const auto&, const auto& mesh)
		{
			list.BindResource("perObject", instanceBuffer, instanceOffset + (entityIndex * sizeof(EntityInstance)));

			// Set the index buffer.
			auto& indexBuffer = device->GetResourceManager().Get(mesh.indexBuffer);
			D3D12_INDEX_BUFFER_VIEW indexView{};
			indexView.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress();
			indexView.SizeInBytes = static_cast<UINT>(indexBuffer.description.size * indexBuffer.description.stride);
			indexView.Format = DXGI_FORMAT_R32_UINT;

			list.Native()->IASetIndexBuffer(&indexView);

			for (const auto& subset : mesh.subsets)
			{
				// Early out if we're a transparent material. We don't want transparent meshes in our depth buffer.
				if (subset.material.transparent)
					continue;

				// #TODO: Only bind once per mesh, and pass subset.vertexOffset into the draw call. This isn't yet supported with DXC, see: https://github.com/microsoft/DirectXShaderCompiler/issues/2907
				list.BindResource("vertexBuffer", mesh.vertexBuffer, subset.vertexOffset * sizeof(Vertex));

				list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, static_cast<uint32_t>(subset.indexOffset), 0, 0);
			}

			++entityIndex;
		});
	});
	
	auto& forwardPass = graph.AddPass("Forward Pass", ExecutionQueue::Graphics);
	const auto outputHDRTag = forwardPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Output HDR sRGB"));
	forwardPass.Read(depthStencilTag, ResourceBind::DSV);
	forwardPass.Read(cameraBufferTag, ResourceBind::CBV);
	forwardPass.Output(outputHDRTag, OutputBind::RTV, true);
	forwardPass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
	{
		// Opaque

		list.BindPipelineState(pipelines["ForwardOpaque"]);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResource("cameraBuffer", resources.GetBuffer(cameraBufferTag));
		list.BindConstants("lightCount", { (uint32_t)device->GetResourceManager().Get(lightBuffer).description.size });
		list.BindResource("lights", lightBuffer);

		{
			VGScopedGPUStat("Opaque", device->GetDirectContext(), list.Native());

			size_t entityIndex = 0;
			registry.view<const TransformComponent, const MeshComponent>().each([&](auto entity, const auto&, const auto& mesh)
			{
				list.BindResource("perObject", instanceBuffer, instanceOffset + (entityIndex * sizeof(EntityInstance)));

				// Set the index buffer.
				auto& indexBuffer = device->GetResourceManager().Get(mesh.indexBuffer);
				D3D12_INDEX_BUFFER_VIEW indexView{};
				indexView.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress();
				indexView.SizeInBytes = static_cast<UINT>(indexBuffer.description.size * indexBuffer.description.stride);
				indexView.Format = DXGI_FORMAT_R32_UINT;

				list.Native()->IASetIndexBuffer(&indexView);

				for (const auto& subset : mesh.subsets)
				{
					// Early out if we're a transparent material. We cannot be drawn in this initial pass.
					if (subset.material.transparent)
						continue;

					// #TODO: Only bind once per mesh, and pass subset.vertexOffset into the draw call. This isn't yet supported with DXC, see: https://github.com/microsoft/DirectXShaderCompiler/issues/2907
					list.BindResource("vertexBuffer", mesh.vertexBuffer, subset.vertexOffset * sizeof(Vertex));

					// Set the material data.
					if (device->GetResourceManager().Valid(subset.material.materialBuffer))
					{
						list.BindResource("material", subset.material.materialBuffer);
					}

					list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, static_cast<uint32_t>(subset.indexOffset), 0, 0);
				}

				++entityIndex;
			});
		}

		// Transparent

		list.BindPipelineState(pipelines["ForwardTransparent"]);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResource("cameraBuffer", resources.GetBuffer(cameraBufferTag));
		list.BindConstants("lightCount", { (uint32_t)device->GetResourceManager().Get(lightBuffer).description.size });
		list.BindResource("lights", lightBuffer);

		{
			VGScopedGPUStat("Transparent", device->GetDirectContext(), list.Native());

			// #TODO: Sort transparent mesh components by distance.
			size_t entityIndex = 0;
			registry.view<const TransformComponent, const MeshComponent>().each([&](auto entity, const auto&, const auto& mesh)
			{
				list.BindResource("perObject", instanceBuffer, instanceOffset + (entityIndex * sizeof(EntityInstance)));

				// Set the index buffer.
				auto& indexBuffer = device->GetResourceManager().Get(mesh.indexBuffer);
				D3D12_INDEX_BUFFER_VIEW indexView{};
				indexView.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress();
				indexView.SizeInBytes = static_cast<UINT>(indexBuffer.description.size * indexBuffer.description.stride);
				indexView.Format = DXGI_FORMAT_R32_UINT;

				list.Native()->IASetIndexBuffer(&indexView);

				for (const auto& subset : mesh.subsets)
				{
					// Early out if we're an opaque material.
					if (!subset.material.transparent)
						continue;

					// #TODO: Only bind once per mesh, and pass subset.vertexOffset into the draw call. This isn't yet supported with DXC, see: https://github.com/microsoft/DirectXShaderCompiler/issues/2907
					list.BindResource("vertexBuffer", mesh.vertexBuffer, subset.vertexOffset * sizeof(Vertex));

					// Set the material data.
					if (device->GetResourceManager().Valid(subset.material.materialBuffer))
					{
						list.BindResource("material", subset.material.materialBuffer);
					}

					list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, static_cast<uint32_t>(subset.indexOffset), 0, 0);
				}

				++entityIndex;
			});
		}
	});

	auto& postProcessPass = graph.AddPass("Post Process Pass", ExecutionQueue::Graphics);
	const auto outputSDRTag = postProcessPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
	}, VGText("Output SDR sRGB"));
	postProcessPass.Read(outputHDRTag, ResourceBind::SRV);
	postProcessPass.Output(outputSDRTag, OutputBind::RTV, true);
	postProcessPass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
	{
		list.BindPipelineState(pipelines["PostProcess"]);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());

		auto outputHDR = resources.GetTexture(outputHDRTag);
		auto& outputHDRComponent = device->GetResourceManager().Get(outputHDR);

		// Set the output texture index.
		list.BindConstants("inputTextures", { outputHDRComponent.SRV->bindlessIndex });

		list.DrawFullscreenQuad();
	});

#if ENABLE_EDITOR
	auto& editorPass = graph.AddPass("Editor Pass", ExecutionQueue::Graphics);
	editorPass.Read(depthStencilTag, ResourceBind::SRV);
	editorPass.Read(outputSDRTag, ResourceBind::SRV);
	editorPass.Output(backBufferTag, OutputBind::RTV, false);
	editorPass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
	{
		userInterface->NewFrame();
		EditorRenderer::Render(device.get(), registry, lastFrameTime, resources.GetTexture(depthStencilTag), resources.GetTexture(outputSDRTag));
		userInterface->Render(list);
	});
#endif

	graph.Build();
	graph.Execute(device.get());

	// #TODO: Move to a present pass.
	device->GetSwapChain()->Present(device->vSync, 0);

	device->AdvanceGPU();
}