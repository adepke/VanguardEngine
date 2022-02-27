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

	list.BindPipelineState(boundsState);
	list.BindConstants("bindData", bindData);
	list.Dispatch(std::ceil(gridInfo.x * gridInfo.y * gridInfo.z / 64.f), 1, 1);
}

ClusteredLightCulling::~ClusteredLightCulling()
{
	device->GetResourceManager().Destroy(clusterBounds);
}

void ClusteredLightCulling::Initialize(RenderDevice* inDevice)
{
	VGScopedCPUStat("Clustered Light Culling Initialize")

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

	ComputePipelineStateDescription boundsStateDesc;
	boundsStateDesc.shader = { "Clusters/ClusterBounds.hlsl", "Main" };
	boundsStateDesc.macros.emplace_back("FROXEL_SIZE", froxelSize);
	boundsState.Build(*device, boundsStateDesc);

	GraphicsPipelineStateDescription depthCullStateDesc;
	depthCullStateDesc.vertexShader = { "Clusters/ClusterDepthCulling.hlsl", "VSMain" };
	depthCullStateDesc.pixelShader = { "Clusters/ClusterDepthCulling.hlsl", "PSMain" };
	depthCullStateDesc.macros.emplace_back("FROXEL_SIZE", froxelSize);
	depthCullStateDesc.blendDescription.AlphaToCoverageEnable = false;
	depthCullStateDesc.blendDescription.IndependentBlendEnable = false;
	depthCullStateDesc.blendDescription.RenderTarget[0].BlendEnable = false;
	depthCullStateDesc.blendDescription.RenderTarget[0].LogicOpEnable = false;
	depthCullStateDesc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	depthCullStateDesc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	depthCullStateDesc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	depthCullStateDesc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	depthCullStateDesc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	depthCullStateDesc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	depthCullStateDesc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	depthCullStateDesc.blendDescription.RenderTarget[0].RenderTargetWriteMask = 0;
	depthCullStateDesc.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	depthCullStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_NONE;  // Transparents can't have back culling.
	depthCullStateDesc.rasterizerDescription.FrontCounterClockwise = false;
	depthCullStateDesc.rasterizerDescription.DepthBias = 0;
	depthCullStateDesc.rasterizerDescription.DepthBiasClamp = 0.f;
	depthCullStateDesc.rasterizerDescription.SlopeScaledDepthBias = 0.f;
	depthCullStateDesc.rasterizerDescription.DepthClipEnable = true;
	depthCullStateDesc.rasterizerDescription.MultisampleEnable = false;
	depthCullStateDesc.rasterizerDescription.AntialiasedLineEnable = false;
	depthCullStateDesc.rasterizerDescription.ForcedSampleCount = 0;
	depthCullStateDesc.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	depthCullStateDesc.depthStencilDescription.DepthEnable = true;
	depthCullStateDesc.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthCullStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;  // Opaque and transparents receive lighting, so they cannot be culled.
	depthCullStateDesc.depthStencilDescription.StencilEnable = false;
	depthCullStateDesc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthCullStateDesc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	depthCullStateDesc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthCullStateDesc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthCullStateDesc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	depthCullState.Build(*device, depthCullStateDesc, false);

	ComputePipelineStateDescription compactionStateDesc;
	compactionStateDesc.shader = { "Clusters/ClusterCompaction.hlsl", "Main" };
	compactionState.Build(*device, compactionStateDesc);

	ComputePipelineStateDescription binningStateDesc;
	binningStateDesc.shader = { "Clusters/ClusterLightBinning.hlsl", "Main" };
	binningStateDesc.macros.emplace_back("MAX_LIGHTS_PER_FROXEL", maxLightsPerFroxel);
	binningState.Build(*device, binningStateDesc);

	ComputePipelineStateDescription indirectGenerationStateDesc;
	indirectGenerationStateDesc.shader = { "Clusters/ClusterIndirectBufferGeneration.hlsl", "Main" };
	indirectGenerationState.Build(*device, indirectGenerationStateDesc);

#if ENABLE_EDITOR
	GraphicsPipelineStateDescription debugOverlayStateDesc{};
	debugOverlayStateDesc.vertexShader = { "Clusters/ClusterDebugOverlay.hlsl", "VSMain" };
	debugOverlayStateDesc.pixelShader = { "Clusters/ClusterDebugOverlay.hlsl", "PSMain" };
	debugOverlayStateDesc.macros.emplace_back("FROXEL_SIZE", froxelSize);
	debugOverlayStateDesc.macros.emplace_back("MAX_LIGHTS_PER_FROXEL", maxLightsPerFroxel);
	debugOverlayStateDesc.blendDescription.AlphaToCoverageEnable = false;
	debugOverlayStateDesc.blendDescription.IndependentBlendEnable = false;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].BlendEnable = false;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].LogicOpEnable = false;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	debugOverlayStateDesc.blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	debugOverlayStateDesc.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	debugOverlayStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_BACK;
	debugOverlayStateDesc.rasterizerDescription.FrontCounterClockwise = false;
	debugOverlayStateDesc.rasterizerDescription.DepthBias = 0;
	debugOverlayStateDesc.rasterizerDescription.DepthBiasClamp = 0.f;
	debugOverlayStateDesc.rasterizerDescription.SlopeScaledDepthBias = 0.f;
	debugOverlayStateDesc.rasterizerDescription.DepthClipEnable = true;
	debugOverlayStateDesc.rasterizerDescription.MultisampleEnable = false;
	debugOverlayStateDesc.rasterizerDescription.AntialiasedLineEnable = false;
	debugOverlayStateDesc.rasterizerDescription.ForcedSampleCount = 0;
	debugOverlayStateDesc.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	debugOverlayStateDesc.depthStencilDescription.DepthEnable = false;
	debugOverlayStateDesc.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	debugOverlayStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_NEVER;  // Opaque and transparents receive lighting, so they cannot be culled.
	debugOverlayStateDesc.depthStencilDescription.StencilEnable = false;
	debugOverlayStateDesc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	debugOverlayStateDesc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	debugOverlayStateDesc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	debugOverlayStateDesc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	debugOverlayStateDesc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	debugOverlayState.Build(*device, debugOverlayStateDesc, false);
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

ClusterResources ClusteredLightCulling::Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil, RenderResource lightsBuffer, RenderResource instanceBuffer, MeshResources meshResources)
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

	auto& clusterDepthCullingPass = graph.AddPass("Cluster Depth Culling", ExecutionQueue::Graphics);
	clusterDepthCullingPass.Read(cameraBuffer, ResourceBind::SRV);
	clusterDepthCullingPass.Read(depthStencil, ResourceBind::DSV);
	clusterDepthCullingPass.Read(instanceBuffer, ResourceBind::SRV);
	clusterDepthCullingPass.Read(meshResources.positionTag, ResourceBind::SRV);
	clusterDepthCullingPass.Read(meshResources.extraTag, ResourceBind::SRV);
	const auto clusterVisibilityTag = clusterDepthCullingPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // Must be static for UAVs.
		.size = gridInfo.x * gridInfo.y * gridInfo.z,
		.format = DXGI_FORMAT_R8_UINT
	}, VGText("Cluster visibility"));
	clusterDepthCullingPass.Write(clusterVisibilityTag, ResourceBind::UAV);
	clusterDepthCullingPass.Bind([&, cameraBuffer, instanceBuffer, meshResources, clusterVisibilityTag](CommandList& list, RenderPassResources& resources)
	{
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(clusterVisibilityTag));

		list.UAVBarrier(resources.GetBuffer(clusterVisibilityTag));
		list.FlushBarriers();

		ClusterDepthCullBindData bindData;
		bindData.objectBuffer = resources.Get(instanceBuffer);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.vertexAssemblyData.positionBuffer = resources.Get(meshResources.positionTag);
		bindData.vertexAssemblyData.extraBuffer = resources.Get(meshResources.extraTag);
		bindData.visibilityBuffer = resources.Get(clusterVisibilityTag);
		bindData.dimensions[0] = gridInfo.x;
		bindData.dimensions[1] = gridInfo.y;
		bindData.dimensions[2] = gridInfo.z;
		bindData.logY = 1.f / std::log(gridInfo.depthFactor);

		list.BindPipelineState(depthCullState);

		MeshSystem::Render(Renderer::Get(), registry, list, false, bindData);
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

		list.BindPipelineState(compactionState);

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
		list.BindPipelineState(indirectGenerationState);
		list.BindConstants("bindData", indirectBindData);
		list.Dispatch(1, 1, 1);
	});

	auto& binningPass = graph.AddPass("Light Binning", ExecutionQueue::Compute);
	binningPass.Read(denseClustersTag, ResourceBind::SRV);
	binningPass.Read(clusterBoundsTag, ResourceBind::SRV);
	binningPass.Read(lightsBuffer, ResourceBind::SRV);
	const auto lightCounterTag = binningPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,
		.size = 1,
		.stride = sizeof(uint32_t)
	}, VGText("Cluster binning light counter"));
	binningPass.Write(lightCounterTag, ResourceBind::UAV);
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
	binningPass.Write(lightInfoTag, ResourceBind::UAV);
	binningPass.Read(indirectBufferTag, ResourceBind::Indirect);
	binningPass.Read(cameraBuffer, ResourceBind::SRV);  // #TODO: Precompute view space light positions.
	binningPass.Bind([&, denseClustersTag, clusterBoundsTag, lightsBuffer, lightCounterTag,
		lightListTag, lightInfoTag, indirectBufferTag, cameraBuffer](CommandList& list, RenderPassResources& resources)
	{
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(lightCounterTag));
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(lightInfoTag));

		list.UAVBarrier(resources.GetBuffer(lightCounterTag));
		list.UAVBarrier(resources.GetBuffer(lightInfoTag));
		list.FlushBarriers();

		list.BindPipelineState(binningState);

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
		bindData.lightCounterBuffer = resources.Get(lightCounterTag);
		bindData.lightListBuffer = resources.Get(lightListTag);
		bindData.lightInfoBuffer = resources.Get(lightInfoTag);

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
		list.BindPipelineState(debugOverlayState);

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