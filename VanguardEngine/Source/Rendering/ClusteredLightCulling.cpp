// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/ClusteredLightCulling.h>
#include <Rendering/Renderer.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/ShaderStructs.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderUtils.h>

ClusterGridInfo ClusteredLightCulling::ComputeGridInfo(const entt::registry& registry) const
{
	// Make sure there's at least one camera.
	if (!registry.size<CameraComponent>())
	{
		return { 0, 0, 0, 0.f };
	}

	const auto& backBufferComponent = device->GetResourceManager().Get(device->GetBackBuffer());

	float cameraNearPlane;
	float cameraFarPlane;
	float cameraFOV;
	registry.view<const CameraComponent>().each([&](auto entity, const auto& camera)
	{
		cameraNearPlane = camera.nearPlane;
		cameraFarPlane = camera.farPlane;
		cameraFOV = camera.fieldOfView;
	});

	const auto x = static_cast<uint32_t>(std::ceil(backBufferComponent.description.width / (float)froxelSize));
	const auto y = static_cast<uint32_t>(std::ceil(backBufferComponent.description.height / (float)froxelSize));
	const float depthFactor = 1.f + (2.f * std::tan(cameraFOV / 4.f) / (float)y);
	const auto z = static_cast<uint32_t>(std::floor(std::log(cameraFarPlane / cameraNearPlane) / std::log(depthFactor)));

	return { x, y, z, depthFactor };
}

void ClusteredLightCulling::ComputeClusterGrid(CommandList& list, uint32_t cameraBuffer, uint32_t clusterBoundsBuffer) const
{
	constexpr auto groupSize = 8;

	const auto& backBufferComponent = device->GetResourceManager().Get(device->GetBackBuffer());

	struct BindData
	{
		int32_t gridDimensionsX;
		int32_t gridDimensionsY;
		int32_t gridDimensionsZ;
		float nearK;
		int32_t resolutionX;
		int32_t resolutionY;
		uint32_t cameraBuffer;
		uint32_t cameraIndex;
		uint32_t boundsBuffer;
	} bindData;

	bindData.gridDimensionsX = gridInfo.x;
	bindData.gridDimensionsY = gridInfo.y;
	bindData.gridDimensionsZ = gridInfo.z;
	bindData.nearK = gridInfo.depthFactor;
	bindData.resolutionX = backBufferComponent.description.width;
	bindData.resolutionY = backBufferComponent.description.height;
	bindData.cameraBuffer = cameraBuffer;
	bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
	bindData.boundsBuffer = clusterBoundsBuffer;

	list.BindPipeline(boundsLayout);
	list.BindConstants("bindData", bindData);
	list.Dispatch(std::ceil(gridInfo.x * gridInfo.y * gridInfo.z / 64.f), 1, 1);
}

ClusteredLightCulling::~ClusteredLightCulling()
{
	device->GetResourceManager().Destroy(clusterBounds);
}

void ClusteredLightCulling::Initialize(RenderDevice* inDevice)
{
	VGScopedCPUStat("Clustered Light Culling Initialize");

	device = inDevice;

	// #TODO: Dynamically reallocate.
	constexpr auto maxDivisionsX = 60;
	constexpr auto maxDivisionsY = 34;
	constexpr auto maxDivisionsZ = 200;

	BufferDescription clusterBoundsDesc{};
	clusterBoundsDesc.updateRate = ResourceFrequency::Static;
	clusterBoundsDesc.bindFlags = BindFlag::UnorderedAccess | BindFlag::ShaderResource;
	clusterBoundsDesc.accessFlags = AccessFlag::GPUWrite;
	clusterBoundsDesc.size = maxDivisionsX * maxDivisionsY * maxDivisionsZ;
	clusterBoundsDesc.stride = 32;

	clusterBounds = device->GetResourceManager().Create(clusterBoundsDesc, VGText("Cluster bounds"));

	boundsLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clusters/ClusterBounds.hlsl", "Main" })
		.Macro({ "FROXEL_SIZE", froxelSize });

	depthCullLayout = RenderPipelineLayout{}
		.VertexShader({ "Clusters/ClusterDepthCulling.hlsl", "VSMain" })
		.PixelShader({ "Clusters/ClusterDepthCulling.hlsl", "PSMain" })
		.CullMode(D3D12_CULL_MODE_NONE)  // Transparents can't have back culling.
		.DepthEnabled(true, false, DepthTestFunction::GreaterEqual)  // Opaque and transparents receive lighting, so they cannot be culled.
		.Macro({ "FROXEL_SIZE", froxelSize });

	compactionLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clusters/ClusterCompaction.hlsl", "Main" });

	binningLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clusters/ClusterLightBinning.hlsl", "Main" })
		.Macro({ "MAX_LIGHTS_PER_FROXEL", maxLightsPerFroxel });

	indirectGenerationLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clusters/ClusterIndirectBufferGeneration.hlsl", "Main" });

#if ENABLE_EDITOR
	debugOverlayLayout = RenderPipelineLayout{}
		.VertexShader({ "Clusters/ClusterDebugOverlay.hlsl", "VSMain" })
		.PixelShader({ "Clusters/ClusterDebugOverlay.hlsl", "PSMain" })
		.DepthEnabled(false)
		.Macro({ "FROXEL_SIZE", froxelSize })
		.Macro({ "MAX_LIGHTS_PER_FROXEL", maxLightsPerFroxel });
#endif

	std::vector<D3D12_INDIRECT_ARGUMENT_DESC> binningIndirectArgDescs;
	binningIndirectArgDescs.emplace_back(D3D12_INDIRECT_ARGUMENT_DESC{
		.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH
	});

	D3D12_COMMAND_SIGNATURE_DESC binningIndirectSignatureDesc{};
	binningIndirectSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
	binningIndirectSignatureDesc.NumArgumentDescs = binningIndirectArgDescs.size();
	binningIndirectSignatureDesc.pArgumentDescs = binningIndirectArgDescs.data();
	binningIndirectSignatureDesc.NodeMask = 0;

	const auto result = device->Native()->CreateCommandSignature(&binningIndirectSignatureDesc, nullptr, IID_PPV_ARGS(binningIndirectSignature.Indirect()));
	if (FAILED(result))
	{
		VGLogError(logRendering, "Failed to create cluster light binning indirect command signature: {}", result);
	}
}

ClusterResources ClusteredLightCulling::Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil, RenderResource lightsBuffer, RenderResource instanceBuffer, MeshResources meshResources, RenderResource meshIndirectRenderArgs)
{
	VGScopedCPUStat("Clustered Light Culling");

	gridInfo = ComputeGridInfo(registry);
	if (gridInfo.x == 0 || gridInfo.y == 0 || gridInfo.z == 0)
	{
		return {};
	}

	const auto clusterBoundsTag = graph.Import(clusterBounds);

	if (dirty)
	{
		auto& computeClusterGridPass = graph.AddPass("Compute Cluster Grid", ExecutionQueue::Compute);
		computeClusterGridPass.Read(cameraBuffer, ResourceBind::SRV);
		computeClusterGridPass.Write(clusterBoundsTag, ResourceBind::UAV);
		computeClusterGridPass.Bind([&, cameraBuffer, clusterBoundsTag](CommandList& list, RenderPassResources& resources)
		{
			ComputeClusterGrid(list, resources.Get(cameraBuffer), resources.Get(clusterBoundsTag));
		});

		dirty = false;
	}

	BufferView clusterVisibilityView{};
	clusterVisibilityView.UAV("uav_visible");
	clusterVisibilityView.UAV("uav_nonvisible", 0, 0, HeapType::NonVisible);

	auto& clusterDepthCullingPass = graph.AddPass("Cluster Depth Culling", ExecutionQueue::Graphics);
	clusterDepthCullingPass.Read(cameraBuffer, ResourceBind::SRV);
	clusterDepthCullingPass.Read(depthStencil, ResourceBind::DSV);
	clusterDepthCullingPass.Read(instanceBuffer, ResourceBind::SRV);
	clusterDepthCullingPass.Read(meshResources.positionTag, ResourceBind::SRV);
	clusterDepthCullingPass.Read(meshIndirectRenderArgs, ResourceBind::Indirect);
	const auto clusterVisibilityTag = clusterDepthCullingPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // Must be static for UAVs.
		.size = gridInfo.x * gridInfo.y * gridInfo.z,
		.format = DXGI_FORMAT_R8_UINT
	}, VGText("Cluster visibility"));
	clusterDepthCullingPass.Write(clusterVisibilityTag, clusterVisibilityView);
	clusterDepthCullingPass.Bind([&, cameraBuffer, instanceBuffer, meshResources, meshIndirectRenderArgs, clusterVisibilityTag](CommandList& list, RenderPassResources& resources)
	{
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(clusterVisibilityTag), resources.Get(clusterVisibilityTag, "uav_visible"), resources.GetDescriptor(clusterVisibilityTag, "uav_nonvisible"));

		list.UAVBarrier(resources.GetBuffer(clusterVisibilityTag));
		list.FlushBarriers();

		struct {
			uint32_t batchId;
			uint32_t objectBuffer;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t vertexPositionBuffer;
			uint32_t visibilityBuffer;
			float logY;
			float padding;
			int32_t dimensions[3];
		} bindData;

		bindData.objectBuffer = resources.Get(instanceBuffer);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.vertexPositionBuffer = resources.Get(meshResources.positionTag);
		bindData.visibilityBuffer = resources.Get(clusterVisibilityTag, "uav_visible");
		bindData.dimensions[0] = gridInfo.x;
		bindData.dimensions[1] = gridInfo.y;
		bindData.dimensions[2] = gridInfo.z;
		bindData.logY = 1.f / std::log(gridInfo.depthFactor);

		list.BindPipeline(depthCullLayout);

		MeshSystem::Render(Renderer::Get(), registry, list, bindData, resources.GetBuffer(meshIndirectRenderArgs));
	});

	auto& clusterCompaction = graph.AddPass("Visible Cluster Compaction", ExecutionQueue::Compute);
	clusterCompaction.Read(clusterVisibilityTag, ResourceBind::SRV);
	const auto denseClustersTag = clusterCompaction.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // UAVs.
		.size = gridInfo.x * gridInfo.y * gridInfo.z,  // Worst case.
		.stride = sizeof(uint32_t),
		.uavCounter = true
	}, VGText("Compacted cluster list"));
	clusterCompaction.Write(denseClustersTag, ResourceBind::UAV);
	const auto indirectBufferTag = clusterCompaction.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // UAVs.
		.size = 1,
		.stride = sizeof(D3D12_DISPATCH_ARGUMENTS)
	}, VGText("Cluster binning indirect argument buffer"));
	clusterCompaction.Write(indirectBufferTag, ResourceBind::UAV);
	clusterCompaction.Bind([&, clusterVisibilityTag, denseClustersTag, indirectBufferTag](CommandList& list, RenderPassResources& resources)
	{
		auto& denseClustersComponent = device->GetResourceManager().Get(resources.GetBuffer(denseClustersTag));

		list.BindPipeline(compactionLayout);

		struct BindData
		{
			int32_t gridDimensionsX;
			int32_t gridDimensionsY;
			int32_t gridDimensionsZ;
			uint32_t visibilityBuffer;
			uint32_t denseListBuffer;
		} bindData;

		bindData.gridDimensionsX = gridInfo.x;
		bindData.gridDimensionsY = gridInfo.y;
		bindData.gridDimensionsZ = gridInfo.z;
		bindData.visibilityBuffer = resources.Get(clusterVisibilityTag);
		bindData.denseListBuffer = resources.Get(denseClustersTag);

		list.BindConstants("bindData", bindData);

		uint32_t dispatchSize = static_cast<uint32_t>(std::ceil(gridInfo.x * gridInfo.y * gridInfo.z / 64.f));
		list.Dispatch(dispatchSize, 1, 1);

		// Ensure that the compaction has finished.
		list.UAVBarrier(resources.GetBuffer(denseClustersTag));
		list.FlushBarriers();

		struct IndirectBindData
		{
			uint32_t denseClusterListBuffer;
			uint32_t indirectBuffer;
		} indirectBindData;

		indirectBindData.denseClusterListBuffer = resources.Get(denseClustersTag);
		indirectBindData.indirectBuffer = resources.Get(indirectBufferTag);

		// Generate the indirect argument buffer.
		list.BindPipeline(indirectGenerationLayout);
		list.BindConstants("bindData", indirectBindData);
		list.Dispatch(1, 1, 1);
	});

	BufferView lightCounterView;
	lightCounterView.UAV("uav_visible");
	lightCounterView.UAV("uav_nonvisible", 0, 0, HeapType::NonVisible);
	BufferView lightInfoView;
	lightInfoView.UAV("uav_visible");
	lightInfoView.UAV("uav_nonvisible", 0, 0, HeapType::NonVisible);

	auto& binningPass = graph.AddPass("Light Binning", ExecutionQueue::Compute);
	binningPass.Read(denseClustersTag, ResourceBind::SRV);
	binningPass.Read(clusterBoundsTag, ResourceBind::SRV);
	binningPass.Read(lightsBuffer, ResourceBind::SRV);
	const auto lightCounterTag = binningPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,
		.size = 1,
		.stride = sizeof(uint32_t)
	}, VGText("Cluster binning light counter"));
	binningPass.Write(lightCounterTag, lightCounterView);
	const auto lightListTag = binningPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,
		.size = gridInfo.x * gridInfo.y * gridInfo.z * maxLightsPerFroxel,
		.stride = sizeof(uint32_t)
	}, VGText("Cluster binning light list"));
	binningPass.Write(lightListTag, ResourceBind::UAV);
	const auto lightInfoTag = binningPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,
		.size = gridInfo.x * gridInfo.y * gridInfo.z,
		.stride = sizeof(uint32_t) * 2
	}, VGText("Cluster grid light info"));
	binningPass.Write(lightInfoTag, lightInfoView);
	binningPass.Read(indirectBufferTag, ResourceBind::Indirect);
	binningPass.Read(cameraBuffer, ResourceBind::SRV);  // #TODO: Precompute view space light positions.
	binningPass.Bind([&, denseClustersTag, clusterBoundsTag, lightsBuffer, lightCounterTag,
		lightListTag, lightInfoTag, indirectBufferTag, cameraBuffer](CommandList& list, RenderPassResources& resources)
	{
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(lightCounterTag), resources.Get(lightCounterTag, "uav_visible"), resources.GetDescriptor(lightCounterTag, "uav_nonvisible"));
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(lightInfoTag), resources.Get(lightInfoTag, "uav_visible"), resources.GetDescriptor(lightInfoTag, "uav_nonvisible"));

		list.UAVBarrier(resources.GetBuffer(lightCounterTag));
		list.UAVBarrier(resources.GetBuffer(lightInfoTag));
		list.FlushBarriers();

		list.BindPipeline(binningLayout);

		auto& lightComponent = device->GetResourceManager().Get(resources.GetBuffer(lightsBuffer));

		struct BindData
		{
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t denseClusterListBuffer;
			uint32_t clusterBoundsBuffer;
			uint32_t lightsBuffer;
			uint32_t lightCount;
			uint32_t lightCounterBuffer;
			uint32_t lightListBuffer;
			uint32_t lightInfoBuffer;
		} bindData;

		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.denseClusterListBuffer = resources.Get(denseClustersTag);
		bindData.clusterBoundsBuffer = resources.Get(clusterBoundsTag);
		bindData.lightsBuffer = resources.Get(lightsBuffer);
		bindData.lightCount = lightComponent.description.size;
		bindData.lightCounterBuffer = resources.Get(lightCounterTag, "uav_visible");
		bindData.lightListBuffer = resources.Get(lightListTag);
		bindData.lightInfoBuffer = resources.Get(lightInfoTag, "uav_visible");

		list.BindConstants("bindData", bindData);

		auto& indirectComponent = device->GetResourceManager().Get(resources.GetBuffer(indirectBufferTag));
		list.Native()->ExecuteIndirect(binningIndirectSignature.Get(), 1, indirectComponent.allocation->GetResource(), 0, nullptr, 0);
	});

	return { lightListTag, lightInfoTag, clusterVisibilityTag };
}

RenderResource ClusteredLightCulling::RenderDebugOverlay(RenderGraph& graph, RenderResource lightInfoBuffer, RenderResource clusterVisibilityBuffer)
{
#if ENABLE_EDITOR
	auto& overlayPass = graph.AddPass("Cluster Debug Overlay", ExecutionQueue::Graphics);
	overlayPass.Read(lightInfoBuffer, ResourceBind::SRV);
	overlayPass.Read(clusterVisibilityBuffer, ResourceBind::SRV);
	const auto clusterDebugOverlayTag = overlayPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Cluster debug overlay"));
	overlayPass.Output(clusterDebugOverlayTag, OutputBind::RTV, LoadType::Preserve);
	overlayPass.Bind([&, lightInfoBuffer, clusterVisibilityBuffer](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipeline(debugOverlayLayout);

		struct BindData
		{
			int32_t gridDimensionsX;
			int32_t gridDimensionsY;
			int32_t gridDimensionsZ;
			float logY;
			uint32_t lightInfoBuffer;
			uint32_t visibilityBuffer;
		} bindData;

		bindData.gridDimensionsX = gridInfo.x;
		bindData.gridDimensionsY = gridInfo.y;
		bindData.gridDimensionsZ = gridInfo.z;
		bindData.logY = 1.f / std::log(gridInfo.depthFactor);
		bindData.lightInfoBuffer = resources.Get(lightInfoBuffer);
		bindData.visibilityBuffer = resources.Get(clusterVisibilityBuffer);

		list.BindConstants("bindData", bindData);

		list.DrawFullscreenQuad();
	});

	return clusterDebugOverlayTag;
#else
	return {};
#endif
}