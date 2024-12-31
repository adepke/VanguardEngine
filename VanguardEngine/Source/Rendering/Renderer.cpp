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
#include <Utility/Math.h>

#include <vector>
#include <utility>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <execution>

void Renderer::CreateRootSignature()
{
	VGScopedCPUStat("Create Indirect Root Signature");

	// Only using a single root signature, so we know all shaders will use the following signature object.
	// This function is just used to aid indirect command signatures, which can require a root signature,
	// and since root signatures are deserialized from shaders at runtime, this doesn't work well with command
	// signatures... so just manually compile some shader here and let the runtime merge the root signature.

	const auto shader = CompileShader(Config::shadersPath / "ClearUAV", ShaderType::Compute, "Main", {});
	if (!shader)
	{
		VGLogCritical(logRendering, "Failed to create indirect root signature shader.");
	}

	const auto result = device->Native()->CreateRootSignature(0, shader->bytecode.data(), shader->bytecode.size(), IID_PPV_ARGS(rootSignature.Indirect()));
	if (FAILED(result))
	{
		VGLogCritical(logRendering, "Failed to create indirect root signature: {}", result);
	}
}

std::vector<MeshRenderable> Renderer::UpdateObjects(const entt::registry& registry)
{
	VGScopedCPUStat("Update Instance Buffer");

	const auto instanceView = registry.view<const TransformComponent, const MeshComponent>();

	std::vector<MeshRenderable> renderables;
	renderables.reserve(instanceView.size_hint());
	std::vector<ObjectData> objectData;
	objectData.reserve(instanceView.size_hint());

	size_t index = 0;
	std::for_each(std::execution::seq, instanceView.begin(), instanceView.end(), [this, &registry, &index, &renderables, &objectData](auto entity)
	{
		const auto& transform = registry.get<TransformComponent>(entity);
		const auto& mesh = registry.get<MeshComponent>(entity);

		for (const auto& subset : mesh.subsets)
		{
			const auto maxScale = std::max(std::max(transform.scale.x, transform.scale.y), transform.scale.z);

			MeshRenderable renderable{
				.positionOffset = (uint32_t)(mesh.globalOffset.position + subset.localOffset.position),
				.extraOffset = (uint32_t)(mesh.globalOffset.extra + subset.localOffset.extra),
				.indexOffset = (uint32_t)(mesh.globalOffset.index + subset.localOffset.index) / sizeof(uint32_t),
				.indexCount = (uint32_t)subset.indices,
				.materialIndex = (uint32_t)subset.materialIndex,
				.batchId = (uint32_t)index,  // every object in separate batch for now...
				.boundingSphereRadius = subset.boundingSphereRadius * maxScale
			};
			renderables.emplace_back(renderable);

			const auto scaling = XMVectorSet(transform.scale.x, transform.scale.y, transform.scale.z, 0.f);
			const auto translation = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);

			const auto scalingMat = XMMatrixScalingFromVector(scaling);
			const auto rotationMat = XMMatrixRotationX(-transform.rotation.x) * XMMatrixRotationY(-transform.rotation.y) * XMMatrixRotationZ(-transform.rotation.z);
			const auto translationMat = XMMatrixTranslationFromVector(translation);

			ObjectData instance;
			instance.worldMatrix = scalingMat * rotationMat * translationMat;
			instance.vertexMetadata = mesh.metadata;
			instance.materialIndex = renderable.materialIndex;
			instance.boundingSphereRadius = renderable.boundingSphereRadius;

			// Apply offsets
			const auto old = instance.vertexMetadata.channelOffsets[0][0];
			for (int i = 0; i < vertexChannels / 4 + 1; ++i)
			{
				instance.vertexMetadata.channelOffsets[i].AddAll(renderable.extraOffset);
			}
			instance.vertexMetadata.channelOffsets[0][0] = old + renderable.positionOffset;

			objectData.emplace_back(instance);

			++index;
		}
	});

	device->GetResourceManager().Write(instanceBuffer, objectData);

	return renderables;
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

	std::vector<Camera> cameras;

	// Standard spectator camera.
	cameras.emplace_back(Camera{
		.position = XMFLOAT4{ translation.x, translation.y, translation.z, 0.f },
		.view = globalViewMatrix,
		.projection = globalProjectionMatrix,
		.inverseView = XMMatrixInverse(nullptr, globalViewMatrix),
		.inverseProjection = XMMatrixInverse(nullptr, globalProjectionMatrix),
		.lastFrameView = globalLastFrameViewMatrix,
		.lastFrameProjection = globalLastFrameProjectionMatrix,
		.lastFrameInverseView = XMMatrixInverse(nullptr, globalLastFrameViewMatrix),
		.lastFrameInverseProjection = XMMatrixInverse(nullptr, globalLastFrameProjectionMatrix),
		.nearPlane = nearPlane,
		.farPlane = farPlane,
		.fieldOfView = fieldOfView,
		.aspectRatio = static_cast<float>(backBuffer.description.width) / static_cast<float>(backBuffer.description.height)
	});

	// Frozen perspective camera.
	const auto translationVector = XMMatrixInverse(nullptr, frozenView).r[3];
	XMStoreFloat3(&translation, translationVector);
	cameras.emplace_back(Camera{
		.position = XMFLOAT4{ translation.x, translation.y, translation.z, 0.f },
		.view = frozenView,
		.projection = frozenProjection,
		.inverseView = XMMatrixInverse(nullptr, frozenView),
		.inverseProjection = XMMatrixInverse(nullptr, frozenProjection),
		.lastFrameView = frozenView,
		.lastFrameProjection = frozenProjection,
		.lastFrameInverseView = XMMatrixInverse(nullptr, frozenView),
		.lastFrameInverseProjection = XMMatrixInverse(nullptr, frozenProjection),
		.nearPlane = nearPlane,
		.farPlane = farPlane,
		.fieldOfView = fieldOfView,
		.aspectRatio = static_cast<float>(backBuffer.description.width) / static_cast<float>(backBuffer.description.height)
	});

	// Sun-view orthographic camera.
	const float sunNearPlane = 1;
	const float sunFarPlane = 50000;
	const float sunHeight = 10000;
	const auto sunRotationMatrix = XMMatrixRotationY(atmosphere.solarZenithAngle);
	const auto sunForward = XMVector4Transform(XMVectorSet(0.f, 0.f, -1.f, 0.f), sunRotationMatrix);
	const auto sunUpward = XMVector4Transform(XMVectorSet(1.f, 0.f, 0.f, 0.f), sunRotationMatrix);
	auto sunPosition = XMVectorSet(0, 0, sunHeight, 0);
	sunPosition = XMVector4Transform(sunPosition, sunRotationMatrix);
	XMFLOAT4 sunPositionFloat;
	XMStoreFloat4(&sunPositionFloat, sunPosition);
	auto sunView = XMMatrixLookAtRH(sunPosition, sunPosition + sunForward, sunUpward);
	const auto viewSize = *CvarGet("sunShadowSize", float);
	auto sunProjection = XMMatrixOrthographicRH(viewSize, viewSize, sunNearPlane, sunFarPlane);
	cameras.emplace_back(Camera{
		.position = sunPositionFloat,
		.view = sunView,
		.projection = sunProjection,
		.inverseView = XMMatrixInverse(nullptr, sunView),
		.inverseProjection = XMMatrixInverse(nullptr, sunProjection),
		.lastFrameView = XMMatrixIdentity(),  // We should never need last frame matrices for this camera.
		.lastFrameProjection = XMMatrixIdentity(),
		.lastFrameInverseView = XMMatrixIdentity(),
		.lastFrameInverseProjection = XMMatrixIdentity(),
		.nearPlane = sunNearPlane,
		.farPlane = sunFarPlane,
		.fieldOfView = 0,
		.aspectRatio = static_cast<float>(backBuffer.description.width) / static_cast<float>(backBuffer.description.height)
	});

	device->GetResourceManager().Write(cameraBuffer, cameras);
}

void Renderer::CreatePipelines()
{
	meshCullLayout = RenderPipelineLayout{}
		.ComputeShader({ "MeshCulling", "Main" });

	prepassLayout = RenderPipelineLayout{}
		.VertexShader({ "Prepass", "VSMain" })
		.DepthEnabled(true, true);

	forwardOpaqueLayout = RenderPipelineLayout{}
		.VertexShader({ "Forward", "VSMain" })
		.PixelShader({ "Forward", "PSMain" })
		.DepthEnabled(true, false, DepthTestFunction::Equal);  // Prepass provides depth.

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

	std::vector<Light> lights{};
	lights.reserve(viewSize);

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

		lights.emplace_back(instance);
		++index;
	});

	device->GetResourceManager().Write(bufferHandle, lights);

	VGAssert(viewSize == index, "Mismatched entity count during buffer creation.");

	return bufferHandle;
}

Renderer::~Renderer()
{
	RenderUtils::Get().Destroy();

	// Sync the device so that resource members in Renderer.h don't get destroyed while in-flight.
	device->Synchronize();
}

void Renderer::Initialize(std::unique_ptr<WindowFrame>&& inWindow, std::unique_ptr<RenderDevice>&& inDevice, entt::registry& registry)
{
	VGScopedCPUStat("Renderer Initialize");

	CvarCreate("meshCulling", "Controls compute-based mesh culling, 0=disabled, 1=frustum, 2=frustum+occlusion", 2);
	CvarCreate("freeze", "Toggles freezing the camera in place, while still allowing for free fly movement. Used for debugging culling", +[]()
	{
		Renderer::Get().FreezeCamera();
	});
	CvarCreate("reloadShaders", "Deletes all shader pipelines, reloads on-demand from disk", +[]()
	{
		Renderer::Get().ReloadShaderPipelines();
	});
	CvarCreate("sunShadowSize", "Width and height of the orthographic camera used for sun shadow mapping, does not affect the shadow map resolution", 50.f);

	constexpr size_t maxVertices = 32 * 1024 * 1024;

	window = std::move(inWindow);
	device = std::move(inDevice);
	meshFactory = std::make_unique<MeshFactory>(device.get(), maxVertices, maxVertices);
	materialFactory = std::make_unique<MaterialFactory>(device.get(), 1024 * 8);
	renderGraphResources.SetDevice(device.get());

	device->CheckFeatureSupport();

	BufferDescription instanceBufferDesc{};
	instanceBufferDesc.updateRate = ResourceFrequency::Dynamic;
	instanceBufferDesc.bindFlags = BindFlag::ShaderResource;
	instanceBufferDesc.accessFlags = AccessFlag::CPUWrite;
	instanceBufferDesc.size = 1024 * 1024 * 8;
	instanceBufferDesc.stride = sizeof(ObjectData);

	instanceBuffer = device->GetResourceManager().Create(instanceBufferDesc, VGText("Instance buffer"));

	BufferDescription cameraBufferDesc{};
	cameraBufferDesc.updateRate = ResourceFrequency::Static;
	cameraBufferDesc.bindFlags = BindFlag::ShaderResource;
	cameraBufferDesc.accessFlags = AccessFlag::CPUWrite;
	cameraBufferDesc.size = 3;  // #TODO: Better camera management.
	cameraBufferDesc.stride = sizeof(Camera);

	cameraBuffer = device->GetResourceManager().Create(cameraBufferDesc, VGText("Camera buffer"));

	userInterface = std::make_unique<UserInterfaceManager>(device.get());

	CreateRootSignature();
	CreatePipelines();

	RenderUtils::Get().Initialize(device.get());

	atmosphere.Initialize(device.get(), registry);
	clusteredCulling.Initialize(device.get());
	ibl.Initialize(device.get());
	bloom.Initialize(device.get());
	occlusionCulling.Initialize(device.get());
	clouds.Initialize(device.get());

	std::vector<D3D12_INDIRECT_ARGUMENT_DESC> meshIndirectArgDescs;
	meshIndirectArgDescs.emplace_back(D3D12_INDIRECT_ARGUMENT_DESC{
		.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT,
		.Constant = {
			.RootParameterIndex = 0,
			.DestOffsetIn32BitValues = 0,
			.Num32BitValuesToSet = 1
		}
	});
	meshIndirectArgDescs.emplace_back(D3D12_INDIRECT_ARGUMENT_DESC{
		.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED
	});

	D3D12_COMMAND_SIGNATURE_DESC meshIndirectSignatureDesc{
		.ByteStride = sizeof(MeshIndirectArgument),
		.NumArgumentDescs = (uint32_t)meshIndirectArgDescs.size(),
		.pArgumentDescs = meshIndirectArgDescs.data(),
		.NodeMask = 0
	};

	const auto result = device->Native()->CreateCommandSignature(&meshIndirectSignatureDesc, rootSignature.Get(), IID_PPV_ARGS(meshIndirectCommandSignature.Indirect()));
	if (FAILED(result))
	{
		VGLogError(logRendering, "Failed to create forward indirect command signature: {}", result);
	}

	meshIndirectRenderArgs = device->GetResourceManager().Create(BufferDescription{
		.updateRate = ResourceFrequency::Static,
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::CPUWrite,
		.size = 1024 * 1024 * 8,
		.stride = sizeof(MeshIndirectArgument),
		.uavCounter = true
	}, VGText("Mesh indirect render argument buffer"));
}

void Renderer::Render(entt::registry& registry)
{
	VGScopedCPUStat("Render");

	// Check if shaders need to be reloaded here since we might be requested at anytime during the frame.
	if (shouldReloadShaders)
	{
		device->Synchronize();
		renderGraphResources.DiscardPipelines();
		shouldReloadShaders = false;
	}

	// Only run once for now since this becomes the bottleneck running every frame with 100k+ objects.
	static bool objectsUpdated = false;
	if (!objectsUpdated)
	{
		objectsUpdated = true;
		const auto renderables = UpdateObjects(registry);
		renderableCount = renderables.size();

		std::vector<MeshIndirectArgument> drawArguments;
		drawArguments.reserve(renderables.size());

		for (const auto& renderable : renderables)
		{
			drawArguments.emplace_back(MeshIndirectArgument{
				.batchId = renderable.batchId,
				.draw = {
					.IndexCountPerInstance = renderable.indexCount,
					.InstanceCount = 1,
					.StartIndexLocation = renderable.indexOffset,
					.BaseVertexLocation = 0,
					.StartInstanceLocation = 0
				}
			});
		}

		device->GetResourceManager().Write(meshIndirectRenderArgs, drawArguments);
	}
	
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
	auto meshIndirectRenderArgsTag = graph.Import(meshIndirectRenderArgs);

	graph.Tag(backBufferTag, ResourceTag::BackBuffer);

	auto lastFrameHiZ = occlusionCulling.GetLastFrameHiZ();

	auto& meshCullPass = graph.AddPass("Mesh Culling Pass", ExecutionQueue::Compute);
	auto meshIndirectCulledRenderArgsTag = meshCullPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // Need unordered-access.
		.size = 1024 * 1024 * 8,
		.stride = sizeof(MeshIndirectArgument),
		.uavCounter = true
	}, VGText("Mesh indirect culled render argument buffer"));
	meshCullPass.Read(meshIndirectRenderArgsTag, ResourceBind::SRV);
	meshCullPass.Write(meshIndirectCulledRenderArgsTag, ResourceBind::UAV);
	meshCullPass.Read(instanceBufferTag, ResourceBind::SRV);
	meshCullPass.Read(cameraBufferTag, ResourceBind::SRV);
	if (*CvarGet("meshCulling", int) > 1 && lastFrameHiZ.id != 0)  // 0 first frame.
		meshCullPass.Read(lastFrameHiZ, ResourceBind::SRV);
	meshCullPass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		const auto meshCulling = *CvarGet("meshCulling", int);

		if (meshCulling > 0)
		{
			list.BindPipeline(meshCullLayout);

			struct {
				uint32_t inputBuffer;
				uint32_t outputBuffer;
				uint32_t objectBuffer;
				uint32_t cameraBuffer;
				uint32_t cameraIndex;
				uint32_t drawCount;
				uint32_t cullingLevel;
				uint32_t hiZTexture;
				uint32_t hiZMipLevels;
			} bindData;

			bindData.inputBuffer = resources.Get(meshIndirectRenderArgsTag);
			bindData.outputBuffer = resources.Get(meshIndirectCulledRenderArgsTag);
			bindData.objectBuffer = resources.Get(instanceBufferTag);
			bindData.cameraBuffer = resources.Get(cameraBufferTag);
			bindData.cameraIndex = cameraFrozen ? 1 : 0;  // #TODO: Support multiple cameras.
			bindData.drawCount = renderableCount;
			bindData.cullingLevel = meshCulling;
			bindData.hiZTexture = (meshCulling > 1 && lastFrameHiZ.id != 0) ? resources.Get(lastFrameHiZ) : 0;
			bindData.hiZMipLevels = *CvarGet("hiZPyramidLevels", int);

			if (lastFrameHiZ.id == 0)
				bindData.cullingLevel = 1;  // Can't use hi-z first frame.

			list.BindConstants("bindData", bindData);

			constexpr auto groupSize = 64;
			const auto dispatchX = std::ceil((float)bindData.drawCount / groupSize);

			list.Dispatch(dispatchX, 1, 1);
		}

		else
		{
			list.Copy(resources.GetBuffer(meshIndirectCulledRenderArgsTag), resources.GetBuffer(meshIndirectRenderArgsTag));

			// Need to update the counter resource as well.

			auto& argsBuffer = device->GetResourceManager().Get(resources.GetBuffer(meshIndirectCulledRenderArgsTag));

			// Transition the counter buffer to copy destination here so we don't transition it in the direct list inside of Write().
			// #TODO: Need to find a much better solution for this.
			list.TransitionBarrier(argsBuffer.counterBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
			list.FlushBarriers();

			device->GetResourceManager().Write(argsBuffer.counterBuffer, (uint32_t)renderableCount);
		}
	});
	
	auto& prePass = graph.AddPass("Prepass", ExecutionQueue::Graphics);
	auto depthStencilTag = prePass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R24G8_TYPELESS
	}, VGText("Depth stencil"));
	prePass.Read(instanceBufferTag, ResourceBind::SRV);
	prePass.Read(cameraBufferTag, ResourceBind::SRV);
	prePass.Read(meshResources.positionTag, ResourceBind::SRV);
	prePass.Read(meshIndirectCulledRenderArgsTag, ResourceBind::Indirect);
	prePass.Output(depthStencilTag, OutputBind::DSV, LoadType::Clear);
	prePass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		struct {
			uint32_t batchId;
			uint32_t objectBuffer;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t vertexPositionBuffer;
		} bindData;

		bindData.objectBuffer = resources.Get(instanceBufferTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.vertexPositionBuffer = resources.Get(meshResources.positionTag);

		list.BindPipeline(prepassLayout);

		MeshSystem::Render(Renderer::Get(), registry, list, bindData, resources.GetBuffer(meshIndirectCulledRenderArgsTag));
	});

	// #TODO: Don't have this here.
	const auto clusterResources = clusteredCulling.Render(graph, registry, cameraBufferTag, depthStencilTag, lightBufferTag, instanceBufferTag, meshResources, meshIndirectCulledRenderArgsTag);
	
	// #TODO: Don't have this here.
	const auto atmosphereResources = atmosphere.ImportResources(graph);
	const auto [luminanceTexture, sunTransmittance] = atmosphere.RenderEnvironmentMap(graph, atmosphereResources, cameraBufferTag);

	// #TODO: Don't have this here.
	const auto iblResources = ibl.UpdateLuts(graph, luminanceTexture, cameraBufferTag);

	// #TODO: Don't have this here.
	occlusionCulling.Render(graph, cameraFrozen, depthStencilTag);

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
	forwardPass.Read(meshIndirectCulledRenderArgsTag, ResourceBind::Indirect);
	forwardPass.Output(outputHDRTag, OutputBind::RTV, LoadType::Clear);
	forwardPass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		ClusterData clusterData;
		auto& gridInfo = clusteredCulling.GetGridInfo();
		clusterData.lightListBuffer = resources.Get(clusterResources.lightList);
		clusterData.lightInfoBuffer = resources.Get(clusterResources.lightInfo);
		clusterData.froxelSize = *CvarGet("clusteredFroxelSize", int);
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
			uint32_t batchId;
			uint32_t objectBuffer;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t materialBuffer;
			uint32_t lightBuffer;
			uint32_t sunTransmittanceBuffer;
			uint32_t vertexPositionBuffer;
			uint32_t vertexExtraBuffer;
			XMFLOAT3 padding;
			ClusterData clusterData;
			IblData iblData;
		} bindData;

		bindData.objectBuffer = resources.Get(instanceBufferTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.vertexPositionBuffer = resources.Get(meshResources.positionTag);
		bindData.vertexExtraBuffer = resources.Get(meshResources.extraTag);
		bindData.materialBuffer = resources.Get(materialBufferTag);
		bindData.lightBuffer = resources.Get(lightBufferTag);
		bindData.clusterData = clusterData;
		bindData.iblData = iblData;
		bindData.sunTransmittanceBuffer = resources.Get(sunTransmittance);

		{
			VGScopedGPUStat("Opaque", device->GetDirectContext(), list.Native());

			list.BindPipeline(forwardOpaqueLayout);

			MeshSystem::Render(Renderer::Get(), registry, list, bindData, resources.GetBuffer(meshIndirectCulledRenderArgsTag));
		}
	});

	// #TODO: Don't have this here.
	const auto cloudResources = clouds.Render(graph, atmosphere, outputHDRTag, cameraBufferTag, depthStencilTag, sunTransmittance);

	// #TODO: Don't have this here.
	atmosphere.Render(graph, atmosphereResources, cloudResources, cameraBufferTag, depthStencilTag, outputHDRTag, registry);

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
	Editor::Get().Render(graph, *device, *this, *graph.resourceManager, registry, cameraBufferTag, depthStencilTag, outputLDRTag, backBufferTag, clusterResources, cloudResources.weather);

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

double Renderer::GetAppTime() const
{
	return appTime;
}

std::pair<uint32_t, uint32_t> Renderer::GetResolution() const
{
	return std::make_pair(device->renderWidth, device->renderHeight);
}

void Renderer::SetResolution(uint32_t width, uint32_t height, bool fullscreen)
{
	device->SetResolution(width, height, fullscreen);

	renderGraphResources.DiscardTransients();
	clusteredCulling.MarkDirty();
}

void Renderer::FreezeCamera()
{
	cameraFrozen = !cameraFrozen;
	if (cameraFrozen)
	{
		frozenView = globalViewMatrix;
		frozenProjection = globalProjectionMatrix;
	}
}

void Renderer::ReloadShaderPipelines()
{
	shouldReloadShaders = true;
}