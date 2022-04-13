// Copyright (c) 2019-2022 Andrew Depke

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

void Renderer::UpdateInstanceBuffer(const entt::registry& registry)
{
	VGScopedCPUStat("Update Instance Buffer");

	const auto instanceView = registry.view<const TransformComponent, const MeshComponent>();
	
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

		device->GetResourceManager().Write(instanceBuffer, instanceData, index * sizeof(EntityInstance));

		++index;
	});
}

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
	prepassLayout = RenderPipelineLayout{}
		.VertexShader({ "Prepass", "VSMain" })
		.DepthEnabled(true, true);

	forwardOpaqueLayout = RenderPipelineLayout{}
		.VertexShader({ "Forward", "VSMain" })
		.PixelShader({ "Forward", "PSMain" })
		.DepthEnabled(true, false, DepthTestFunction::Equal)  // Prepass provides depth.
		.Macro({ "FROXEL_SIZE", clusteredCulling.froxelSize });

	postProcessLayout = RenderPipelineLayout{}
		.VertexShader({ "PostProcess", "VSMain" })
		.PixelShader({ "PostProcess", "PSMain" })
		.DepthEnabled(false);
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
	materialFactory = std::make_unique<MaterialFactory>(device.get(), 1024 * 8);

	device->CheckFeatureSupport();

	BufferDescription instanceBufferDesc{};
	instanceBufferDesc.updateRate = ResourceFrequency::Dynamic;
	instanceBufferDesc.bindFlags = BindFlag::ShaderResource;
	instanceBufferDesc.accessFlags = AccessFlag::CPUWrite;
	instanceBufferDesc.size = 1024 * 64;
	instanceBufferDesc.stride = sizeof(EntityInstance);

	instanceBuffer = device->GetResourceManager().Create(instanceBufferDesc, VGText("Instance buffer"));

	BufferDescription cameraBufferDesc{};
	cameraBufferDesc.updateRate = ResourceFrequency::Static;
	cameraBufferDesc.bindFlags = BindFlag::ConstantBuffer | BindFlag::ShaderResource;
	cameraBufferDesc.accessFlags = AccessFlag::CPUWrite;
	cameraBufferDesc.size = 1;  // #TODO: Support multiple cameras.
	cameraBufferDesc.stride = sizeof(Camera);

	cameraBuffer = device->GetResourceManager().Create(cameraBufferDesc, VGText("Camera buffer"));

	userInterface = std::make_unique<UserInterfaceManager>(device.get());

	CreatePipelines();

	RenderUtils::Get().Initialize(device.get());

	atmosphere.Initialize(device.get(), registry);
	clusteredCulling.Initialize(device.get());
	ibl.Initialize(device.get());
	bloom.Initialize(device.get());
}

void Renderer::Render(entt::registry& registry)
{
	VGScopedCPUStat("Render");

	UpdateInstanceBuffer(registry);
	UpdateCameraBuffer(registry);

	RenderGraph graph{ &renderGraphResources };
	
	const auto lightBuffer = CreateLightBuffer(registry);

	MeshResources meshResources;
	meshResources.positionTag = graph.Import(meshFactory->vertexPositionBuffer);
	meshResources.extraTag = graph.Import(meshFactory->vertexExtraBuffer);

	RenderResource materialBufferTag = graph.Import(materialFactory->materialBuffer);

	auto backBufferTag = graph.Import(device->GetBackBuffer());
	auto cameraBufferTag = graph.Import(cameraBuffer);
	auto instanceBufferTag = graph.Import(instanceBuffer);
	auto lightBufferTag = graph.Import(lightBuffer);

	graph.Tag(backBufferTag, ResourceTag::BackBuffer);
	
	auto& prePass = graph.AddPass("Prepass", ExecutionQueue::Graphics);
	auto depthStencilTag = prePass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R24G8_TYPELESS
	}, VGText("Depth stencil"));
	prePass.Read(instanceBufferTag, ResourceBind::SRV);
	prePass.Read(cameraBufferTag, ResourceBind::SRV);
	prePass.Read(meshResources.positionTag, ResourceBind::SRV);
	prePass.Read(meshResources.extraTag, ResourceBind::SRV);
	prePass.Output(depthStencilTag, OutputBind::DSV, LoadType::Clear);
	prePass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		struct {
			uint32_t objectBuffer;
			uint32_t objectIndex;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			VertexAssemblyData vertexAssemblyData;
		} bindData;

		bindData.objectBuffer = resources.Get(instanceBufferTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.vertexAssemblyData.positionBuffer = resources.Get(meshResources.positionTag);
		bindData.vertexAssemblyData.extraBuffer = resources.Get(meshResources.extraTag);

		list.BindPipeline(prepassLayout);

		MeshSystem::Render(Renderer::Get(), registry, list, bindData);
	});

	// #TODO: Don't have this here.
	const auto clusterResources = clusteredCulling.Render(graph, registry, cameraBufferTag, depthStencilTag, lightBufferTag, instanceBufferTag, meshResources);
	
	// #TODO: Don't have this here.
	const auto atmosphereResources = atmosphere.ImportResources(graph);
	const auto [luminanceTexture, sunTransmittance] = atmosphere.RenderEnvironmentMap(graph, atmosphereResources, cameraBufferTag);

	// #TODO: Don't have this here.
	const auto iblResources = ibl.UpdateLuts(graph, luminanceTexture, cameraBufferTag);

	auto& forwardPass = graph.AddPass("Forward Pass", ExecutionQueue::Graphics);
	const auto outputHDRTag = forwardPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Output HDR sRGB"));
	forwardPass.Read(depthStencilTag, ResourceBind::DSV);
	forwardPass.Read(instanceBufferTag, ResourceBind::SRV);
	forwardPass.Read(cameraBufferTag, ResourceBind::SRV);
	forwardPass.Read(lightBufferTag, ResourceBind::SRV);
	forwardPass.Read(meshResources.positionTag, ResourceBind::SRV);
	forwardPass.Read(meshResources.extraTag, ResourceBind::SRV);
	forwardPass.Read(materialBufferTag, ResourceBind::SRV);
	forwardPass.Read(clusterResources.lightList, ResourceBind::SRV);
	forwardPass.Read(clusterResources.lightInfo, ResourceBind::SRV);
	forwardPass.Read(iblResources.irradianceTag, ResourceBind::SRV);
	forwardPass.Read(iblResources.prefilterTag, ResourceBind::SRV);
	forwardPass.Read(iblResources.brdfTag, ResourceBind::SRV);
	forwardPass.Read(sunTransmittance, ResourceBind::SRV);
	forwardPass.Output(outputHDRTag, OutputBind::RTV, LoadType::Clear);
	forwardPass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		ClusterData clusterData;
		auto& gridInfo = clusteredCulling.GetGridInfo();
		clusterData.lightListBuffer = resources.Get(clusterResources.lightList);
		clusterData.lightInfoBuffer = resources.Get(clusterResources.lightInfo);
		clusterData.dimensions[0] = gridInfo.x;
		clusterData.dimensions[1] = gridInfo.y;
		clusterData.dimensions[2] = gridInfo.z;
		clusterData.logY = 1.f / std::log(gridInfo.depthFactor);

		IblData iblData;
		iblData.irradianceTexture = resources.Get(iblResources.irradianceTag);
		iblData.prefilterTexture = resources.Get(iblResources.prefilterTag);
		iblData.brdfTexture = resources.Get(iblResources.brdfTag);
		iblData.prefilterLevels = ibl.GetPrefilterLevels();

		struct {
			uint32_t objectBuffer;
			uint32_t objectIndex;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			VertexAssemblyData vertexAssemblyData;
			uint32_t materialBuffer;
			uint32_t materialIndex;
			uint32_t lightBuffer;
			uint32_t sunTransmittanceBuffer;
			ClusterData clusterData;
			IblData iblData;
		} bindData;

		bindData.objectBuffer = resources.Get(instanceBufferTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.vertexAssemblyData.positionBuffer = resources.Get(meshResources.positionTag);
		bindData.vertexAssemblyData.extraBuffer = resources.Get(meshResources.extraTag);
		bindData.materialBuffer = resources.Get(materialBufferTag);
		bindData.lightBuffer = resources.Get(lightBufferTag);
		bindData.clusterData = clusterData;
		bindData.iblData = iblData;
		bindData.sunTransmittanceBuffer = resources.Get(sunTransmittance);

		{
			VGScopedGPUStat("Opaque", device->GetDirectContext(), list.Native());

			list.BindPipeline(forwardOpaqueLayout);

			MeshSystem::Render(Renderer::Get(), registry, list, bindData);
		}
	});

	// #TODO: Don't have this here.
	atmosphere.Render(graph, atmosphereResources, cameraBufferTag, depthStencilTag, outputHDRTag, registry);

	// #TODO: Don't have this here.
	bloom.Render(graph, outputHDRTag);

	auto& postProcessPass = graph.AddPass("Post Process Pass", ExecutionQueue::Graphics);
	const auto outputLDRTag = postProcessPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
	}, VGText("Output LDR sRGB"));
	postProcessPass.Read(outputHDRTag, ResourceBind::SRV);
	postProcessPass.Output(outputLDRTag, OutputBind::RTV, LoadType::Clear);
	postProcessPass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipeline(postProcessLayout);
		list.BindConstants("bindData", { resources.Get(outputHDRTag) });

		list.DrawFullscreenQuad();
	});

	// #TODO: Don't have this here.
	Editor::Get().Render(graph, *device, *this, *graph.resourceManager, registry, cameraBufferTag, depthStencilTag, outputLDRTag, backBufferTag, clusterResources);

	auto& presentPass = graph.AddPass("Present", ExecutionQueue::Graphics);
	presentPass.Read(backBufferTag, ResourceBind::Common);
	presentPass.Bind([](CommandList& list, RenderPassResources& resources)
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