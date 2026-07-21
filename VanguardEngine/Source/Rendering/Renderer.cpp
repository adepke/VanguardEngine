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
#include <Rendering/Object.h>
#include <Core/Config.h>
#include <Rendering/RenderUtils.h>
#include <Rendering/DebugDraw.h>
#include <Rendering/TextureCapture.h>
#include <Editor/Editor.h>
#include <Utility/Math.h>

#include <vector>
#include <utility>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <optional>
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

void Renderer::OnMeshConstruct(entt::registry& registry, entt::entity entity)
{
	// Can't be certain that the transform component is ready to go yet, so defer until next frame.
	pendingMeshes.emplace_back(entity);
}

void Renderer::OnMeshDestroy(entt::registry& registry, entt::entity entity)
{
	registry.remove<GpuSlotComponent>(entity);
}

void Renderer::OnSlotDestroy(entt::registry& registry, entt::entity entity)
{
	const auto& slot = registry.get<GpuSlotComponent>(entity);
	instanceBufferAllocator.Free(slot.baseSlot, slot.count);
	drawArgsDirty = true;
}

void Renderer::OnTransformDirty(entt::registry& registry, entt::entity entity)
{
	// When a transform component is modified, attach a dirty tag so we can bulk update these entities.
	registry.emplace_or_replace<TransformDirtyComponent>(entity);
}

void Renderer::UpdateGpuScene(entt::registry& registry)
{
	VGScopedCPUStat("Update GPU Scene");

	// New meshes need slot allocation.
	for (const auto entity : pendingMeshes)
	{
		// Sanity check since state could've changed before we got to it.
		if (!registry.valid(entity) || !registry.all_of<MeshComponent>(entity) || registry.all_of<GpuSlotComponent>(entity))
		{
			continue;
		}

		const auto& mesh = registry.get<MeshComponent>(entity);
		const auto subsetCount = (uint32_t)mesh.subsets.size();
		if (subsetCount == 0)
		{
			continue;
		}

		const auto baseSlot = instanceBufferAllocator.Allocate(subsetCount);
		if (baseSlot == SlotAllocator::invalidSlot)
		{
			VGLogError(logRendering, "Out of instance buffer space, mesh will not be rendered.");
			continue;
		}

		registry.emplace<GpuSlotComponent>(entity, baseSlot, subsetCount);
		registry.emplace_or_replace<TransformDirtyComponent>(entity);  // Needs initial upload.
		drawArgsDirty = true;
	}

	pendingMeshes.clear();

	// If a mesh was added or removed, need to rebuild the indirect draw args.
	if (drawArgsDirty)
	{
		std::vector<MeshIndirectArgument> drawArguments;

		const auto slotView = registry.view<const MeshComponent, const GpuSlotComponent>();
		slotView.each([&drawArguments](const auto& mesh, const auto& slot)
		{
			for (size_t i = 0; i < mesh.subsets.size(); ++i)
			{
				const auto& subset = mesh.subsets[i];

				drawArguments.emplace_back(MeshIndirectArgument{
					.batchId = slot.baseSlot + (uint32_t)i,
					.draw = {
						.IndexCountPerInstance = (uint32_t)subset.indices,
						.InstanceCount = 1,
						.StartIndexLocation = (uint32_t)(mesh.globalOffset.index + subset.localOffset.index) / sizeof(uint32_t),
						.BaseVertexLocation = 0,
						.StartInstanceLocation = 0
					}
				});
			}
		});

		renderableCount = drawArguments.size();

		if (!drawArguments.empty())
		{
			device->GetResourceManager().Write(meshIndirectRenderArgs, drawArguments);
		}

		drawArgsDirty = false;
	}

	// Upload instance data for dirty objects only.
	{
		VGScopedCPUStat("Update Instance Buffer");

		const auto dirtyView = registry.view<const TransformDirtyComponent, const GpuSlotComponent, const TransformComponent, const MeshComponent>();

		// Walk all dirty objects and compute the new object data for the GPU scene.
		std::vector<std::pair<uint32_t, ObjectData>> updates;
		dirtyView.each([this, &updates](const auto& slot, const auto& transform, const auto& mesh)
		{
			for (size_t i = 0; i < mesh.subsets.size(); ++i)
			{
				updates.emplace_back(slot.baseSlot + (uint32_t)i, BuildObjectData(transform, mesh, i));
			}
		});

		if (!updates.empty())
		{
			// Sort and then collapse adjacent ranges to perform bulk writes instead of individual per-object writes.
			std::sort(updates.begin(), updates.end(), [](const auto& left, const auto& right) { return left.first < right.first; });

			size_t rangeBegin = 0;
			while (rangeBegin < updates.size())
			{
				size_t rangeEnd = rangeBegin + 1;
				while (rangeEnd < updates.size() && updates[rangeEnd].first == updates[rangeEnd - 1].first + 1)
				{
					++rangeEnd;
				}

				std::vector<ObjectData> objectData;
				objectData.reserve(rangeEnd - rangeBegin);
				for (size_t i = rangeBegin; i < rangeEnd; ++i)
				{
					objectData.emplace_back(updates[i].second);
				}

				device->GetResourceManager().Write(instanceBuffer, objectData, updates[rangeBegin].first * sizeof(ObjectData));

				rangeBegin = rangeEnd;
			}
		}

		// All meshes uploaded, nothing is dirty.
		registry.clear<TransformDirtyComponent>();
	}
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

	XMFLOAT3 lastFrameTranslation;
	auto lastFrameTranslationVector = XMMatrixInverse(nullptr, globalLastFrameViewMatrix).r[3];
	XMStoreFloat3(&lastFrameTranslation, lastFrameTranslationVector);

	// Standard spectator camera.
	cameras.emplace_back(Camera{
		.position = XMFLOAT4{ translation.x, translation.y, translation.z, 0.f },
		.view = globalViewMatrix,
		.projection = globalProjectionMatrix,
		.inverseView = XMMatrixInverse(nullptr, globalViewMatrix),
		.inverseProjection = XMMatrixInverse(nullptr, globalProjectionMatrix),
		.lastFramePosition = XMFLOAT4{ lastFrameTranslation.x, lastFrameTranslation.y, lastFrameTranslation.z, 0.f },
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
		.lastFramePosition = XMFLOAT4{ translation.x, translation.y, translation.z, 0.f },
		.lastFrameView = frozenView,
		.lastFrameProjection = frozenProjection,
		.lastFrameInverseView = XMMatrixInverse(nullptr, frozenView),
		.lastFrameInverseProjection = XMMatrixInverse(nullptr, frozenProjection),
		.nearPlane = nearPlane,
		.farPlane = farPlane,
		.fieldOfView = fieldOfView,
		.aspectRatio = static_cast<float>(backBuffer.description.width) / static_cast<float>(backBuffer.description.height)
	});

	float solarZenithAngle = 0.f;
	if (registry.valid(atmosphere.sunLight))
	{
		solarZenithAngle = registry.get<TimeOfDayComponent>(atmosphere.sunLight).solarZenithAngle;
	}

	// Sun-view orthographic camera. In kilometers instead of meters for precision. Shadow map is not accurate otherwise.
	const float sunNearPlane = 1;
	const float sunFarPlane = 50000 / 1000.f;
	const float sunHeight = 10000 / 1000.f;
	const auto sunRotationMatrix = XMMatrixRotationY(solarZenithAngle);
	const auto sunForward = XMVector4Transform(XMVectorSet(0.f, 0.f, -1.f, 0.f), sunRotationMatrix);
	const auto sunUpward = XMVector4Transform(XMVectorSet(1.f, 0.f, 0.f, 0.f), sunRotationMatrix);
	auto sunPosition = XMVectorSet(0, 0, sunHeight, 0);
	sunPosition = XMVector4Transform(sunPosition, sunRotationMatrix);
	XMFLOAT4 sunPositionFloat;
	XMStoreFloat4(&sunPositionFloat, sunPosition);
	auto sunView = XMMatrixLookAtRH(sunPosition, sunPosition + sunForward, sunUpward);
	const auto viewSize = 100.f;
	auto sunProjection = XMMatrixOrthographicRH(viewSize, viewSize, sunNearPlane, sunFarPlane);
	cameras.emplace_back(Camera{
		.position = sunPositionFloat,
		.view = sunView,
		.projection = sunProjection,
		.inverseView = XMMatrixInverse(nullptr, sunView),
		.inverseProjection = XMMatrixInverse(nullptr, sunProjection),
		.lastFramePosition = sunPositionFloat,
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
		.PixelShader({ "Prepass", "PSMain" })  // PS for normals.
		.DepthEnabled(true, true);

	forwardOpaqueLayout = RenderPipelineLayout{}
		.VertexShader({ "Forward", "VSMain" })
		.PixelShader({ "Forward", "PSMain" })
		.DepthEnabled(true, false, DepthTestFunction::Equal);  // Prepass provides depth.
}

BufferHandle Renderer::CreateLightBuffer(const entt::registry& registry)
{
	VGScopedCPUStat("Create Light Buffer");

	const auto lightView = registry.view<const TransformComponent, const LightComponent>();
	const auto viewSize = std::max(lightView.size_hint(), 1ull);  // Prevent a zero-sized buffer from being created.

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

	// If no lights exist, create a dummy light that does nothing.
	// This is a bit of a hack since the resource manager cannot create empty resources,
	// and the render graph can't handle null resources - so we have to make something.
	// There's almost certainly a better solution here.
	if (index == 0)
	{
		lights.emplace_back(Light{
			.position = { 0.f, 0.f, 0.f },
			.type = static_cast<uint32_t>(LightType::Point),
			.color = { 0.f, 0.f, 0.f },
			.luminance = 0.f
		});
		++index;
	}

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
	CvarCreate("exposure", "Linear scene exposure multiplier applied before tone mapping", 8.5f);
	// Keep in sync with ToneMapping.hlsli
	CvarCreate("toneMapper", "Selects the tone mapping operator (0=disabled, 1=ACES Hill, 2=ACES Narkowicz, 3=AgX, 4=Khronos PBR Neutral, 5=Reinhard)", 4);
	CvarCreate("referenceGridEnabled", "Controls if the reference grid is visible", 0);
	
	constexpr size_t maxVertices = 64 * 1024 * 1024;
	constexpr size_t maxObjectSlots = 1024 * 1024;

	window = std::move(inWindow);
	device = std::move(inDevice);
	meshFactory = std::make_unique<MeshFactory>(device.get(), maxVertices, maxVertices);
	materialFactory = std::make_unique<MaterialFactory>(device.get(), 1024 * 8);
	renderGraphResources.SetDevice(device.get());

	device->CheckFeatureSupport();

	// Hook up all entity component change notifications to keep the GPU scene in sync.
	// #TODO: refactor into dedicated system, renderer is already massive.
	registry.on_construct<MeshComponent>().connect<&Renderer::OnMeshConstruct>(this);
	registry.on_destroy<MeshComponent>().connect<&Renderer::OnMeshDestroy>(this);
	registry.on_destroy<GpuSlotComponent>().connect<&Renderer::OnSlotDestroy>(this);
	registry.on_update<TransformComponent>().connect<&Renderer::OnTransformDirty>(this);
	registry.on_construct<TransformComponent>().connect<&Renderer::OnTransformDirty>(this);

	BufferDescription instanceBufferDesc{};
	instanceBufferDesc.updateRate = ResourceFrequency::Static;
	instanceBufferDesc.bindFlags = BindFlag::ShaderResource;
	instanceBufferDesc.accessFlags = AccessFlag::CPUWrite;
	instanceBufferDesc.size = maxObjectSlots;
	instanceBufferDesc.stride = sizeof(ObjectData);

	instanceBuffer = device->GetResourceManager().Create(instanceBufferDesc, VGText("Instance buffer"));
	instanceBufferAllocator.Initialize(maxObjectSlots);  // instanceBuffer is the backing storage.

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
	accelerationStructures.Initialize(device.get());
	rayTracedShadows.Initialize(device.get());
	DebugDraw::Get().Initialize(device.get());

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

	UpdateGpuScene(registry);
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
		.format = DXGI_FORMAT_R32_TYPELESS  // Note: can switch to R24G8 if I ever want the stencil, 24 bits is plenty.
	}, VGText("Depth stencil"));
	auto geometricNormalsTag = prePass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16_FLOAT  // Octahedral-encoded, world space.
	}, VGText("Geometric normals"));
	prePass.Read(instanceBufferTag, ResourceBind::SRV);
	prePass.Read(cameraBufferTag, ResourceBind::SRV);
	prePass.Read(meshResources.positionTag, ResourceBind::SRV);
	prePass.Read(meshResources.extraTag, ResourceBind::SRV);
	prePass.Read(meshIndirectCulledRenderArgsTag, ResourceBind::Indirect);
	prePass.Output(depthStencilTag, OutputBind::DSV, LoadType::Clear);
	prePass.Output(geometricNormalsTag, OutputBind::RTV, LoadType::Clear);
	prePass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		struct {
			uint32_t batchId;
			uint32_t objectBuffer;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t vertexPositionBuffer;
			uint32_t vertexExtraBuffer;
		} bindData;

		bindData.objectBuffer = resources.Get(instanceBufferTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.vertexPositionBuffer = resources.Get(meshResources.positionTag);
		bindData.vertexExtraBuffer = resources.Get(meshResources.extraTag);

		list.BindPipeline(prepassLayout);

		MeshSystem::Render(Renderer::Get(), registry, list, bindData, resources.GetBuffer(meshIndirectCulledRenderArgsTag));
	});

	// #TODO: Don't have this here.
	const auto clusterResources = clusteredCulling.Render(graph, registry, cameraBufferTag, depthStencilTag, lightBufferTag, instanceBufferTag, meshResources, meshIndirectCulledRenderArgsTag);

	const auto asResources = accelerationStructures.Render(graph, registry, *meshFactory, meshResources.positionTag);
	std::optional<RenderResource> sunShadowTag;
	if (*CvarGet("rayTracingEnabled", int) != 0 && *CvarGet("rtShadowsEnabled", int) != 0)
	{
		// #TODO: lots of different and inconsistent ways of dealing with the sun, and if there's no sun. Fix this.
		XMFLOAT3 sunDirection;
		bool foundSun = false;
		registry.view<const TransformComponent, const LightComponent>().each([&](auto entity, const auto& transform, const auto& light)
		{
			if (!foundSun && light.type == LightType::Directional)
			{
				const auto direction = XMVector3Rotate(XMVectorSet(1.f, 0.f, 0.f, 0.f), XMQuaternionRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, -transform.rotation.z));
				XMStoreFloat3(&sunDirection, XMVectorNegate(direction));  // Direction towards the sun.
				foundSun = true;
			}
		});

		if (foundSun)
		{
			sunShadowTag = rayTracedShadows.Render(graph, asResources.tlasTag, depthStencilTag, geometricNormalsTag, cameraBufferTag, sunDirection, appFrame);
		}
	}
	
	// #TODO: Don't have this here.
	const auto atmosphereResources = atmosphere.ImportResources(graph);
	const auto [luminanceTexture, atmosphereIrradiance] = atmosphere.RenderEnvironmentMap(graph, atmosphereResources, cameraBufferTag, registry, clouds.coverage);

	// #TODO: Don't have this here.
	occlusionCulling.Render(graph, cameraFrozen, depthStencilTag);

	// #TODO: Don't have this here.
	// Note clouds must run before IBL since it contributes to the luminance cube.
	const auto cloudResources = clouds.Render(graph, registry, atmosphere, cameraBufferTag, depthStencilTag, atmosphereIrradiance, luminanceTexture);

	// After all environment map contributions are done, build the mip chain and prepare for IBL convolution.
	atmosphere.GenerateLuminanceMips(graph, luminanceTexture);

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
	forwardPass.Read(atmosphereIrradiance, ResourceBind::SRV);
	forwardPass.Read(cloudResources.weather, ResourceBind::SRV);
	forwardPass.Read(meshIndirectCulledRenderArgsTag, ResourceBind::Indirect);
	if (sunShadowTag)
	{
		forwardPass.Read(*sunShadowTag, ResourceBind::SRV);
	}
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
			uint32_t vertexPositionBuffer;
			uint32_t vertexExtraBuffer;
			uint32_t materialBuffer;
			uint32_t lightBuffer;
			uint32_t atmosphereIrradianceBuffer;
			float globalWeatherCoverage;
			uint32_t weatherTexture;
			uint32_t sunShadowTexture;
			ClusterData clusterData;
			IblData iblData;
			uint32_t outputResolution[2];
		} bindData;

		bindData.objectBuffer = resources.Get(instanceBufferTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.vertexPositionBuffer = resources.Get(meshResources.positionTag);
		bindData.vertexExtraBuffer = resources.Get(meshResources.extraTag);
		bindData.materialBuffer = resources.Get(materialBufferTag);
		bindData.lightBuffer = resources.Get(lightBufferTag);
		bindData.atmosphereIrradianceBuffer = resources.Get(atmosphereIrradiance);
		bindData.globalWeatherCoverage = clouds.coverage;  // #TODO: Scale by precipitation?
		bindData.weatherTexture = resources.Get(cloudResources.weather);
		bindData.sunShadowTexture = sunShadowTag ? resources.Get(*sunShadowTag) : 0;
		bindData.clusterData = clusterData;
		bindData.iblData = iblData;

		const auto& outputComponent = device->GetResourceManager().Get(resources.GetTexture(outputHDRTag));
		bindData.outputResolution[0] = outputComponent.description.width;
		bindData.outputResolution[1] = outputComponent.description.height;

		{
			VGScopedGPUStat("Opaque", device->GetDirectContext(), list.Native());

			list.BindPipeline(forwardOpaqueLayout);

			MeshSystem::Render(Renderer::Get(), registry, list, bindData, resources.GetBuffer(meshIndirectCulledRenderArgsTag));
		}
	});

	// #TODO: Don't have this here.
	atmosphere.Render(graph, clouds, atmosphereResources, cloudResources, cameraBufferTag, depthStencilTag, outputHDRTag, registry);

	// #TODO: Don't have this here.
	bloom.Render(graph, outputHDRTag);

	// #TODO: unify with other debug overlays, have the editor control this not a cvar.
	if (*CvarGet("rtDebugView", int) > 0)
	{
		rayTracedShadows.RenderDebug(graph, asResources.tlasTag, cameraBufferTag, outputHDRTag, *CvarGet("rtDebugView", int));
	}

	auto& postProcessPass = graph.AddPass("Post Process Pass", ExecutionQueue::Graphics);
	const auto outputLDRTag = postProcessPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
	}, VGText("Output LDR sRGB"));
	postProcessPass.Read(outputHDRTag, ResourceBind::SRV);
	postProcessPass.Output(outputLDRTag, OutputBind::RTV, LoadType::Clear);
	postProcessPass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		auto postProcessLayout = RenderPipelineLayout{}
			.VertexShader({ "PostProcess", "VSMain" })
			.PixelShader({ "PostProcess", "PSMain" })
			.DepthEnabled(false);

		list.BindPipeline(postProcessLayout);

		struct {
			uint32_t mainOutput;
			uint32_t toneMapper;
			float exposure;
			float padding;
		} bindData;

		bindData.mainOutput = resources.Get(outputHDRTag);
		bindData.toneMapper = static_cast<uint32_t>(*CvarGet("toneMapper", int));
		bindData.exposure = *CvarGet("exposure", float);
		list.BindConstants("bindData", bindData);

		list.DrawFullscreenQuad();
	});

	// #TODO: Don't have this here.
	auto& gridPass = graph.AddPass("Reference Grid Pass", ExecutionQueue::Graphics, *CvarGet("referenceGridEnabled", int) != 0);
	gridPass.Read(cameraBufferTag, ResourceBind::SRV);
	gridPass.Read(depthStencilTag, ResourceBind::DSV);
	gridPass.Output(outputLDRTag, OutputBind::RTV, LoadType::Preserve);
	gridPass.Bind([&](CommandList& list, RenderPassResources& resources)
	{
		BlendMode alphaBlend{
			.srcBlend = D3D12_BLEND_SRC_ALPHA,
			.destBlend = D3D12_BLEND_INV_SRC_ALPHA,
			.blendOp = D3D12_BLEND_OP_ADD,
			.srcBlendAlpha = D3D12_BLEND_ONE,
			.destBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
			.blendOpAlpha = D3D12_BLEND_OP_ADD,
		};

		// Depth tested, read only.
		const auto gridLayout = RenderPipelineLayout{}
			.VertexShader({ "ReferenceGrid", "VSMain" })
			.PixelShader({ "ReferenceGrid", "PSMain" })
			.BlendMode(true, alphaBlend)
			.CullMode(D3D12_CULL_MODE_NONE)
			.DepthEnabled(true, false, DepthTestFunction::Greater);

		list.BindPipeline(gridLayout);

		struct {
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			float gridHeightMeters;
			float majorCellMeters;
			float minorCellMeters;
			float gridAlpha;
			float fadeStartMeters;
			float fadeEndMeters;
			XMFLOAT3 gridColor;
			float padding;
		} bindData;

		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.gridHeightMeters = 100.0;  // Offset a bit off the planet surface, since precision breaks down.
		bindData.majorCellMeters = 1000.0;  // Kilometer squares.
		bindData.minorCellMeters = 100.0;  // 100-meter squares.
		bindData.gridAlpha = 0.5;
		bindData.fadeStartMeters = 2000.0;
		bindData.fadeEndMeters = 8000.0;
		bindData.gridColor = { 0.7, 0.7, 0.7 };
		
		list.BindConstants("bindData", bindData);

		list.DrawFullscreenQuad();
	});

	// #TODO: Don't have this here.
	DebugDraw::Get().Render(graph, cameraBufferTag, depthStencilTag, outputLDRTag);

	// #TODO: Don't have this here.
	Editor::Get().Render(graph, *device, *this, *graph.resourceManager, registry, cameraBufferTag, depthStencilTag, outputLDRTag, backBufferTag, clusterResources, cloudResources.weather);

	// #TODO: bundle this into the present pass?
	if (capturePending)
	{
		auto& capturePass = graph.AddPass("Frame Capture", ExecutionQueue::Graphics);
		capturePass.Read(outputLDRTag, ResourceBind::Common);
		// Similar to the present pass, use the back buffer to ensure this pass runs at the end of the graph.
		capturePass.Read(backBufferTag, ResourceBind::Common);
		capturePass.Bind([&](CommandList& list, RenderPassResources& resources)
		{
			pendingCapture = TextureCapture::Enqueue(*device, list, resources.GetTexture(outputLDRTag));
		});
	}

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

	if (capturePending)
	{
		// Lists executed, now safe the resolve.
		const auto pngBytes = TextureCapture::Resolve(*device, pendingCapture);
		lastCaptureSucceeded = TextureCapture::WritePngFile(capturePath, pngBytes);
		if (lastCaptureSucceeded)
		{
			VGLog(logRendering, "Captured frame to '{}'.", capturePath.generic_wstring());
		}
		else
		{
			VGLogError(logRendering, "Failed to capture frame to '{}'.", capturePath.generic_wstring());
		}
		capturePending = false;
	}

	Editor::Get().Update(*device, registry);

	appFrame++;
}

double Renderer::GetAppTime() const
{
	return appTime;
}

uint32_t Renderer::GetAppFrame() const
{
	return appFrame;
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

void Renderer::ResetAppTime()
{
	appTime = 0;
}

void Renderer::RequestCapture(const std::filesystem::path& path)
{
	capturePath = path;
	capturePending = true;
	lastCaptureSucceeded = false;
}
