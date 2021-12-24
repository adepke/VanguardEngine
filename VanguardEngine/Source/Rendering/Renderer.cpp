// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/Renderer.h>
#include <Rendering/Resource.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/ShaderStructs.h>
#include <Core/Config.h>
#include <Rendering/RenderUtils.h>
#include <Editor/Editor.h>

#include <vector>
#include <utility>
#include <cstring>
#include <cmath>

void Renderer::UpdateCameraBuffer(const entt::registry& registry)
{
	VGScopedCPUStat("Update Camera Buffer");

	XMFLOAT3 translation;
	float nearPlane;
	float farPlane;
	float fieldOfView;
	registry.view<const TransformComponent, const CameraComponent>().each([&](auto entity, const auto& transform, const auto& camera)
	{
		// #TODO: Support more than one camera.
		translation = transform.translation;
		nearPlane = camera.nearPlane;
		farPlane = camera.farPlane;
		fieldOfView = camera.fieldOfView;
	});

	auto& backBuffer = device->GetResourceManager().Get(device->GetBackBuffer());

	Camera bufferData;
	bufferData.position = XMFLOAT4{ translation.x, translation.y, translation.z, 0.f };
	bufferData.view = globalViewMatrix;
	bufferData.projection = globalProjectionMatrix;
	bufferData.inverseView = XMMatrixInverse(nullptr, bufferData.view);
	bufferData.inverseProjection = XMMatrixInverse(nullptr, bufferData.projection);
	bufferData.nearPlane = nearPlane;
	bufferData.farPlane = farPlane;
	bufferData.fieldOfView = fieldOfView;
	bufferData.aspectRatio = static_cast<float>(backBuffer.description.width) / static_cast<float>(backBuffer.description.height);

	std::vector<uint8_t> byteData;
	byteData.resize(sizeof(Camera));
	std::memcpy(byteData.data(), &bufferData, sizeof(bufferData));

	device->GetResourceManager().Write(cameraBuffer, byteData);
}

void Renderer::CreatePipelines()
{
	GraphicsPipelineStateDescription prepassStateDesc;

	prepassStateDesc.vertexShader = { "Prepass_VS", "main" };

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

	GraphicsPipelineStateDescription forwardStateDesc;

	forwardStateDesc.vertexShader = { "Default_VS", "main" };
	forwardStateDesc.pixelShader = { "Default_PS", "main" };
	forwardStateDesc.macros.emplace_back("FROXEL_SIZE", clusteredCulling.froxelSize);

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

	GraphicsPipelineStateDescription atmosphereStateDesc;

	atmosphereStateDesc.vertexShader = { "Atmosphere/Atmosphere_VS", "main" };
	atmosphereStateDesc.pixelShader = { "Atmosphere/Atmosphere_PS", "main" };

	atmosphereStateDesc.blendDescription.AlphaToCoverageEnable = false;
	atmosphereStateDesc.blendDescription.IndependentBlendEnable = false;
	atmosphereStateDesc.blendDescription.RenderTarget[0].BlendEnable = false;
	atmosphereStateDesc.blendDescription.RenderTarget[0].LogicOpEnable = false;
	atmosphereStateDesc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	atmosphereStateDesc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	atmosphereStateDesc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	atmosphereStateDesc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	atmosphereStateDesc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	atmosphereStateDesc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	atmosphereStateDesc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	atmosphereStateDesc.blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	atmosphereStateDesc.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	atmosphereStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_BACK;
	atmosphereStateDesc.rasterizerDescription.FrontCounterClockwise = false;
	atmosphereStateDesc.rasterizerDescription.DepthBias = 0;
	atmosphereStateDesc.rasterizerDescription.DepthBiasClamp = 0.f;
	atmosphereStateDesc.rasterizerDescription.SlopeScaledDepthBias = 0.f;
	atmosphereStateDesc.rasterizerDescription.DepthClipEnable = true;
	atmosphereStateDesc.rasterizerDescription.MultisampleEnable = false;
	atmosphereStateDesc.rasterizerDescription.AntialiasedLineEnable = false;
	atmosphereStateDesc.rasterizerDescription.ForcedSampleCount = 0;
	atmosphereStateDesc.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	atmosphereStateDesc.depthStencilDescription.DepthEnable = true;
	atmosphereStateDesc.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	atmosphereStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;  // Draw where the depth buffer is at the clear value.
	atmosphereStateDesc.depthStencilDescription.StencilEnable = false;
	atmosphereStateDesc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	atmosphereStateDesc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	atmosphereStateDesc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	atmosphereStateDesc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	atmosphereStateDesc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pipelines.AddGraphicsState(device.get(), "Atmosphere", atmosphereStateDesc, false);

	GraphicsPipelineStateDescription postProcessStateDesc;

	postProcessStateDesc.vertexShader = { "PostProcess_VS", "main" };
	postProcessStateDesc.pixelShader = { "PostProcess_PS", "main" };

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
	const auto viewSize = instanceView.size_hint();
	const auto [bufferHandle, bufferOffset] = device->FrameAllocate(sizeof(EntityInstance) * viewSize);

	size_t index = 0;
	instanceView.each([&](auto entity, const auto& transform, const auto&)
	{
		const auto scaling = XMVectorSet(transform.scale.x, transform.scale.y, transform.scale.z, 0.f);
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
	const auto viewSize = lightView.size_hint();

	BufferDescription lightBufferDescription;
	lightBufferDescription.updateRate = ResourceFrequency::Dynamic;
	lightBufferDescription.bindFlags = BindFlag::ShaderResource;
	lightBufferDescription.accessFlags = AccessFlag::CPUWrite;
	lightBufferDescription.size = viewSize;
	lightBufferDescription.stride = sizeof(Light);

	const auto bufferHandle = device->GetResourceManager().Create(lightBufferDescription, VGText("Light buffer"));
	device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), bufferHandle);

	std::vector<uint8_t> instanceData{};
	instanceData.resize(viewSize * sizeof(Light));

	size_t index = 0;
	lightView.each([&](auto entity, const auto& transform, const auto& light)
	{
		const auto direction = XMVector3Rotate(XMVectorSet(1.f, 0.f, 0.f, 0.f), XMQuaternionRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, -transform.rotation.z));
		XMFLOAT3 directionUnpacked;
		XMStoreFloat3(&directionUnpacked, direction);

		Light instance{
			.position = transform.translation,
			.type = static_cast<uint32_t>(light.type),
			.color = light.color,
			.luminance = 1.f,  // #TEMP
			.direction = directionUnpacked
		};

		std::memcpy(instanceData.data() + index * sizeof(instance), &instance, sizeof(instance));

		++index;
	});

	device->GetResourceManager().Write(bufferHandle, instanceData);

	VGAssert(viewSize == index, "Mismatched entity count during buffer creation.");

	return bufferHandle;
}

Renderer::~Renderer()
{
	// Sync the device so that resource members in Renderer.h don't get destroyed while in-flight.
	device->Synchronize();
}

void Renderer::Initialize(std::unique_ptr<WindowFrame>&& inWindow, std::unique_ptr<RenderDevice>&& inDevice, entt::registry& registry)
{
	VGScopedCPUStat("Renderer Initialize");

	constexpr size_t maxVertices = 32 * 1024 * 1024;

	window = std::move(inWindow);
	device = std::move(inDevice);
	meshFactory = std::make_unique<MeshFactory>(device.get(), maxVertices, maxVertices);

	device->CheckFeatureSupport();

	BufferDescription cameraBufferDesc{};
	cameraBufferDesc.updateRate = ResourceFrequency::Static;
	cameraBufferDesc.bindFlags = BindFlag::ConstantBuffer;
	cameraBufferDesc.accessFlags = AccessFlag::CPUWrite;
	cameraBufferDesc.size = 1;  // #TODO: Support multiple cameras.
	cameraBufferDesc.stride = sizeof(Camera);

	cameraBuffer = device->GetResourceManager().Create(cameraBufferDesc, VGText("Camera buffer"));

	userInterface = std::make_unique<UserInterfaceManager>(device.get());

	nullDescriptor = device->AllocateDescriptor(DescriptorType::Default);

	D3D12_SHADER_RESOURCE_VIEW_DESC nullViewDesc{};
	nullViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nullViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	nullViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	device->Native()->CreateShaderResourceView(nullptr, &nullViewDesc, nullDescriptor);

	CreatePipelines();

	RenderUtils::Get().Initialize(device.get());

	atmosphere.Initialize(device.get(), registry);
	clusteredCulling.Initialize(device.get());
	ibl.Initialize(device.get());
}

void Renderer::Render(entt::registry& registry)
{
	VGScopedCPUStat("Render");

	// Update the camera buffer immediately.
	UpdateCameraBuffer(registry);

	RenderGraph graph{ &renderGraphResources };
	
	const auto instanceData = CreateInstanceBuffer(registry);
	instanceBuffer = instanceData.first;
	instanceOffset = instanceData.second;
	const auto lightBuffer = CreateLightBuffer(registry);

	auto backBufferTag = graph.Import(device->GetBackBuffer());
	auto cameraBufferTag = graph.Import(cameraBuffer);
	auto lightBufferTag = graph.Import(lightBuffer);

	graph.Tag(backBufferTag, ResourceTag::BackBuffer);
	
	auto& prePass = graph.AddPass("Prepass", ExecutionQueue::Graphics);
	auto depthStencilTag = prePass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R24G8_TYPELESS
	}, VGText("Depth stencil"));
	prePass.Read(cameraBufferTag, ResourceBind::CBV);
	prePass.Output(depthStencilTag, OutputBind::DSV, LoadType::Clear);
	prePass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
	{
		list.BindPipelineState(pipelines["Prepass"]);
		list.BindResource("camera", resources.GetBuffer(cameraBufferTag));

		MeshSystem::Render(Renderer::Get(), registry, list, false);
	});

	// #TODO: Don't have this here.
	const auto clusterResources = clusteredCulling.Render(graph, registry, cameraBufferTag, depthStencilTag, lightBufferTag);
	
	// #TODO: Don't have this here.
	const auto atmosphereResources = atmosphere.ImportResources(graph);
	const auto luminanceTexture = atmosphere.RenderEnvironmentMap(graph, atmosphereResources, cameraBufferTag);

	// #TODO: Don't have this here.
	const auto iblResources = ibl.UpdateLuts(graph, luminanceTexture, cameraBufferTag);

	auto& forwardPass = graph.AddPass("Forward Pass", ExecutionQueue::Graphics);
	const auto outputHDRTag = forwardPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Output HDR sRGB"));
	forwardPass.Read(depthStencilTag, ResourceBind::DSV);
	forwardPass.Read(cameraBufferTag, ResourceBind::CBV);
	forwardPass.Read(lightBufferTag, ResourceBind::SRV);
	forwardPass.Read(clusterResources.lightList, ResourceBind::SRV);
	forwardPass.Read(clusterResources.lightInfo, ResourceBind::SRV);
	forwardPass.Read(iblResources.irradianceTag, ResourceBind::SRV);
	forwardPass.Read(iblResources.prefilterTag, ResourceBind::SRV);
	forwardPass.Read(iblResources.brdfTag, ResourceBind::SRV);
	forwardPass.Output(outputHDRTag, OutputBind::RTV, LoadType::Clear);
	forwardPass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
	{
		// Opaque

		list.BindPipelineState(pipelines["ForwardOpaque"]);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("textureCubes", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResource("camera", resources.GetBuffer(cameraBufferTag));
		list.BindResource("lights", resources.GetBuffer(lightBufferTag));
		list.BindResource("clusteredLightList", resources.GetBuffer(clusterResources.lightList));
		list.BindResource("clusteredLightInfo", resources.GetBuffer(clusterResources.lightInfo));

		struct ClusterData
		{
			uint32_t dimensionsX;
			uint32_t dimensionsY;
			uint32_t dimensionsZ;
			float logY;
		} clusterData;

		auto& gridInfo = clusteredCulling.GetGridInfo();

		clusterData.dimensionsX = gridInfo.x;
		clusterData.dimensionsY = gridInfo.y;
		clusterData.dimensionsZ = gridInfo.z;
		clusterData.logY = 1.f / std::log(gridInfo.depthFactor);

		std::vector<uint32_t> clusterBufferData;
		clusterBufferData.resize(sizeof(clusterData) / 4);
		std::memcpy(clusterBufferData.data(), &clusterData, clusterBufferData.size() * sizeof(uint32_t));
		list.BindConstants("clusterData", clusterBufferData);

		struct IblData
		{
			uint32_t irradianceTexture;
			uint32_t prefilterTexture;
			uint32_t brdfTexture;
			uint32_t prefilterLevels;
		} iblData;

		const auto& irradianceComponent = device->GetResourceManager().Get(resources.GetTexture(iblResources.irradianceTag));
		
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = irradianceComponent.description.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = -1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.f;

		auto irradianceSRV = device->AllocateDescriptor(DescriptorType::Default);
		device->Native()->CreateShaderResourceView(irradianceComponent.allocation->GetResource(), &srvDesc, irradianceSRV);

		const auto& prefilterComponent = device->GetResourceManager().Get(resources.GetTexture(iblResources.prefilterTag));

		srvDesc.Format = prefilterComponent.description.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = -1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.f;

		auto prefilterSRV = device->AllocateDescriptor(DescriptorType::Default);
		device->Native()->CreateShaderResourceView(prefilterComponent.allocation->GetResource(), &srvDesc, prefilterSRV);

		const auto& brdfComponent = device->GetResourceManager().Get(resources.GetTexture(iblResources.brdfTag));

		srvDesc.Format = brdfComponent.description.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = -1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

		auto brdfSRV = device->AllocateDescriptor(DescriptorType::Default);
		device->Native()->CreateShaderResourceView(brdfComponent.allocation->GetResource(), &srvDesc, brdfSRV);

		iblData.irradianceTexture = irradianceSRV.bindlessIndex;
		iblData.prefilterTexture = prefilterSRV.bindlessIndex;
		iblData.brdfTexture = brdfSRV.bindlessIndex;
		iblData.prefilterLevels = ibl.GetPrefilterLevels();
		list.BindConstants("iblData", iblData);

		device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(irradianceSRV));
		device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(prefilterSRV));
		device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(brdfSRV));

		{
			VGScopedGPUStat("Opaque", device->GetDirectContext(), list.Native());

			MeshSystem::Render(Renderer::Get(), registry, list, false);
		}

		// Transparent

		list.BindPipelineState(pipelines["ForwardTransparent"]);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResource("camera", resources.GetBuffer(cameraBufferTag));
		list.BindResource("lights", resources.GetBuffer(lightBufferTag));
		list.BindResource("clusteredLightList", resources.GetBuffer(clusterResources.lightList));
		list.BindResource("clusteredLightInfo", resources.GetBuffer(clusterResources.lightInfo));
		list.BindConstants("clusterData", clusterBufferData);
		list.BindConstants("iblData", iblData);

		{
			VGScopedGPUStat("Transparent", device->GetDirectContext(), list.Native());

			// #TODO: Sort transparent mesh components by distance.
			MeshSystem::Render(Renderer::Get(), registry, list, true);
		}
	});

	// #TODO: Don't have this here.
	atmosphere.Render(graph, atmosphereResources, pipelines, cameraBufferTag, depthStencilTag, outputHDRTag, registry);

	auto& postProcessPass = graph.AddPass("Post Process Pass", ExecutionQueue::Graphics);
	const auto outputLDRTag = postProcessPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
	}, VGText("Output LDR sRGB"));
	postProcessPass.Read(outputHDRTag, ResourceBind::SRV);
	postProcessPass.Output(outputLDRTag, OutputBind::RTV, LoadType::Clear);
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

	// #TODO: Don't have this here.
	Editor::Get().Render(graph, *device, *this, registry, cameraBufferTag, depthStencilTag, outputLDRTag, backBufferTag, clusterResources);

	auto& presentPass = graph.AddPass("Present", ExecutionQueue::Graphics);
	presentPass.Read(backBufferTag, ResourceBind::Common);
	presentPass.Bind([](CommandList& list, RenderGraphResourceManager& resources)
	{
		// We can't call present here since it would execute during pass recording.
		// #TODO: Try to find a better solution for this.
		//device->Present();
	});

	graph.Build();
	graph.Execute(device.get());

	device->Present();
	device->AdvanceGPU();

	Editor::Get().Update();
}

std::pair<uint32_t, uint32_t> Renderer::GetResolution() const
{
	return std::make_pair(device->renderWidth, device->renderHeight);
}

void Renderer::SetResolution(uint32_t width, uint32_t height, bool fullscreen)
{
	device->SetResolution(width, height, fullscreen);

	renderGraphResources.DiscardTransients(device.get());
	clusteredCulling.MarkDirty();
}