// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/ClusteredLightCulling.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/ShaderStructs.h>
#include <Rendering/RenderComponents.h>
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

void ClusteredLightCulling::ComputeClusterGrid(CommandList& list, BufferHandle cameraBuffer, BufferHandle clusterBoundsBuffer) const
{
	constexpr auto groupSize = 8;

	const auto& backBufferComponent = device->GetResourceManager().Get(device->GetBackBuffer());

	struct ClusterData
	{
		int32_t gridDimensionsX;
		int32_t gridDimensionsY;
		int32_t gridDimensionsZ;
		float nearK;
		int32_t resolutionX;
		int32_t resolutionY;
		XMFLOAT2 padding;
	} clusterData;

	clusterData.gridDimensionsX = gridInfo.x;
	clusterData.gridDimensionsY = gridInfo.y;
	clusterData.gridDimensionsZ = gridInfo.z;
	clusterData.nearK = gridInfo.depthFactor;
	clusterData.resolutionX = backBufferComponent.description.width;
	clusterData.resolutionY = backBufferComponent.description.height;

	std::vector<uint32_t> clusterConstants;
	clusterConstants.resize(sizeof(ClusterData) / 4);
	std::memcpy(clusterConstants.data(), &clusterData, clusterConstants.size() * sizeof(uint32_t));

	list.BindPipelineState(boundsState);
	list.BindConstants("clusterData", clusterConstants);
	list.BindResource("clusterBounds", clusterBoundsBuffer);
	list.BindResource("camera", cameraBuffer);
	list.Native()->Dispatch(std::ceil(gridInfo.x * gridInfo.y * gridInfo.z / 64.f), 1, 1);

	list.UAVBarrier(clusterBoundsBuffer);
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
	boundsStateDesc.shader = { "ClusterBounds.hlsl", "ComputeClusterBoundsMain" };
	boundsStateDesc.macros.emplace_back("FROXEL_SIZE", froxelSize);
	boundsState.Build(*device, boundsStateDesc);

	GraphicsPipelineStateDescription depthCullStateDesc;
	depthCullStateDesc.vertexShader = { "ClusterDepthCulling.hlsl", "VSMain" };
	depthCullStateDesc.pixelShader = { "ClusterDepthCulling.hlsl", "PSMain" };
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
	compactionStateDesc.shader = { "ClusterCompaction.hlsl", "ComputeDenseClusterListMain" };
	compactionState.Build(*device, compactionStateDesc);

	ComputePipelineStateDescription binningStateDesc;
	binningStateDesc.shader = { "ClusterLightBinning.hlsl", "ComputeLightBinsMain" };
	binningStateDesc.macros.emplace_back("MAX_LIGHTS_PER_FROXEL", maxLightsPerFroxel);
	binningState.Build(*device, binningStateDesc);

#if ENABLE_EDITOR
	GraphicsPipelineStateDescription debugOverlayStateDesc{};
	debugOverlayStateDesc.vertexShader = { "ClusterDebugOverlay.hlsl", "VSMain" };
	debugOverlayStateDesc.pixelShader = { "ClusterDebugOverlay.hlsl", "PSMain" };
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
}

ClusterResources ClusteredLightCulling::Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil, BufferHandle instanceBuffer, size_t instanceOffset, BufferHandle lights)
{
	VGScopedCPUStat("Clustered Light Culling");
	VGScopedGPUStat("Clustered Light Culling", device->GetDirectContext(), device->GetDirectList().Native());

	gridInfo = ComputeGridInfo(registry);
	if (gridInfo.x == 0 || gridInfo.y == 0 || gridInfo.z == 0)
	{
		return {};
	}

	const auto clusterBoundsTag = graph.Import(clusterBounds);

	if (dirty)
	{
		auto& computeClusterGridPass = graph.AddPass("Compute Cluster Grid", ExecutionQueue::Compute);
		computeClusterGridPass.Read(cameraBuffer, ResourceBind::CBV);
		computeClusterGridPass.Write(clusterBoundsTag, ResourceBind::UAV);
		computeClusterGridPass.Bind([&, cameraBuffer, clusterBoundsTag](CommandList& list, RenderGraphResourceManager& resources)
		{
			ComputeClusterGrid(list, resources.GetBuffer(cameraBuffer), resources.GetBuffer(clusterBoundsTag));
		});

		dirty = false;
	}

	auto& clusterDepthCullingPass = graph.AddPass("Cluster Depth Culling", ExecutionQueue::Graphics);
	clusterDepthCullingPass.Read(cameraBuffer, ResourceBind::CBV);
	clusterDepthCullingPass.Read(depthStencil, ResourceBind::DSV);
	const auto clusterVisibilityTag = clusterDepthCullingPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // Must be static for UAVs.
		.size = gridInfo.x * gridInfo.y * gridInfo.z,
		.stride = sizeof(bool) * 4  // Structured buffers pad each element out to 4 bytes.
	}, VGText("Cluster visibility"));
	clusterDepthCullingPass.Write(clusterVisibilityTag, ResourceBind::UAV);
	clusterDepthCullingPass.Bind([&, cameraBuffer, clusterVisibilityTag, instanceBuffer, instanceOffset](CommandList& list, RenderGraphResourceManager& resources)
	{
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(clusterVisibilityTag));

		list.UAVBarrier(resources.GetBuffer(clusterVisibilityTag));
		list.FlushBarriers();

		list.BindPipelineState(depthCullState);
		list.BindResource("camera", resources.GetBuffer(cameraBuffer));
		list.BindResource("clusterVisibility", resources.GetBuffer(clusterVisibilityTag));

		struct ClusterData
		{
			int32_t gridDimensionsX;
			int32_t gridDimensionsY;
			int32_t gridDimensionsZ;
			float logY;
		} clusterData;

		clusterData.gridDimensionsX = gridInfo.x;
		clusterData.gridDimensionsY = gridInfo.y;
		clusterData.gridDimensionsZ = gridInfo.z;
		clusterData.logY = 1.f / std::log(gridInfo.depthFactor);

		std::vector<uint32_t> clusterConstants;
		clusterConstants.resize(sizeof(ClusterData) / 4);
		std::memcpy(clusterConstants.data(), &clusterData, clusterConstants.size() * sizeof(uint32_t));

		list.BindConstants("clusterData", clusterConstants);

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
				// #TODO: Only bind once per mesh, and pass subset.vertexOffset into the draw call. This isn't yet supported with DXC, see: https://github.com/microsoft/DirectXShaderCompiler/issues/2907
				list.BindResource("vertexBuffer", mesh.vertexBuffer, subset.vertexOffset * sizeof(Vertex));

				list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, static_cast<uint32_t>(subset.indexOffset), 0, 0);
			}

			++entityIndex;
		});
	});

	auto& clusterCompaction = graph.AddPass("Visible Cluster Compaction", ExecutionQueue::Compute);
	const auto denseClustersTag = clusterCompaction.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // UAVs.
		.size = gridInfo.x * gridInfo.y * gridInfo.z,  // Worst case.
		.stride = sizeof(uint32_t),
		.uavCounter = true
	}, VGText("Compacted cluster list"));
	clusterCompaction.Read(clusterVisibilityTag, ResourceBind::SRV);
	clusterCompaction.Write(denseClustersTag, ResourceBind::UAV);
	clusterCompaction.Bind([&, clusterVisibilityTag, denseClustersTag](CommandList& list, RenderGraphResourceManager& resources)
	{
		auto& denseClustersComponent = device->GetResourceManager().Get(resources.GetBuffer(denseClustersTag));

		list.BindPipelineState(compactionState);
		list.BindResource("clusterVisibility", resources.GetBuffer(clusterVisibilityTag));
		list.BindResourceTable("denseClusterList", *denseClustersComponent.UAV);

		struct ClusterData
		{
			int32_t gridDimensionsX;
			int32_t gridDimensionsY;
			int32_t gridDimensionsZ;
			float padding;
		} clusterData;

		clusterData.gridDimensionsX = gridInfo.x;
		clusterData.gridDimensionsY = gridInfo.y;
		clusterData.gridDimensionsZ = gridInfo.z;

		std::vector<uint32_t> clusterConstants;
		clusterConstants.resize(sizeof(ClusterData) / 4);
		std::memcpy(clusterConstants.data(), &clusterData, clusterConstants.size() * sizeof(uint32_t));

		list.BindConstants("clusterData", clusterConstants);

		uint32_t dispatchSize = static_cast<uint32_t>(std::ceil(gridInfo.x * gridInfo.y * gridInfo.z / 64.f));
		list.Native()->Dispatch(dispatchSize, 1, 1);
	});

	auto& lightsBufferComponent = device->GetResourceManager().Get(lights);

	auto& binningPass = graph.AddPass("Light Binning", ExecutionQueue::Compute);
	binningPass.Read(denseClustersTag, ResourceBind::SRV);
	binningPass.Read(clusterBoundsTag, ResourceBind::SRV);
	//binningPass.Read(lightsBuffer, ResourceBind::SRV);
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
	// #TEMP
	binningPass.Read(cameraBuffer, ResourceBind::CBV);
	binningPass.Write(lightInfoTag, ResourceBind::UAV);
	binningPass.Bind([&, denseClustersTag, lightCounterTag,
		lightListTag, lightInfoTag, lights, cameraBuffer](CommandList& list, RenderGraphResourceManager& resources)
	{
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(lightCounterTag));
		RenderUtils::Get().ClearUAV(list, resources.GetBuffer(lightInfoTag));

		list.UAVBarrier(resources.GetBuffer(lightCounterTag));
		list.UAVBarrier(resources.GetBuffer(lightInfoTag));
		list.FlushBarriers();

		list.BindPipelineState(binningState);
		list.BindResource("denseClusterList", resources.GetBuffer(denseClustersTag));
		list.BindResource("clusterBounds", clusterBounds);
		list.BindResource("lights", lights);
		list.BindResource("lightCounter", resources.GetBuffer(lightCounterTag));
		list.BindResource("lightList", resources.GetBuffer(lightListTag));
		list.BindResource("clusterLightInfo", resources.GetBuffer(lightInfoTag));

		// #TEMP: Testing
		auto& b = device->GetResourceManager().Get(resources.GetBuffer(denseClustersTag));
		list.BindResource("testing", b.counterBuffer);
		list.BindResource("camera", resources.GetBuffer(cameraBuffer));

		auto& lightComponent = device->GetResourceManager().Get(lights);
		list.BindConstants("lightCount", { (uint32_t)lightComponent.description.size });

		// #TODO: Indirect dispatch, send only as many thread groups as the number of non-culled clusters.
		list.Native()->Dispatch(gridInfo.x * gridInfo.y * gridInfo.z, 1, 1);
	});

	return { lightListTag, lightInfoTag, clusterVisibilityTag };
}

RenderResource ClusteredLightCulling::RenderDebugOverlay(RenderGraph& graph, RenderResource lightInfoBuffer, RenderResource clusterVisibilityBuffer)
{
	auto& overlayPass = graph.AddPass("Cluster Debug Overlay", ExecutionQueue::Graphics);
	overlayPass.Read(lightInfoBuffer, ResourceBind::SRV);
	overlayPass.Read(clusterVisibilityBuffer, ResourceBind::SRV);
	const auto clusterDebugOverlayTag = overlayPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Cluster debug overlay"));
	overlayPass.Output(clusterDebugOverlayTag, OutputBind::RTV, false);
	overlayPass.Bind([&, lightInfoBuffer, clusterVisibilityBuffer](CommandList& list, RenderGraphResourceManager& resources)
	{
		list.BindPipelineState(debugOverlayState);
		list.BindResource("clusterLightInfo", resources.GetBuffer(lightInfoBuffer));
		list.BindResource("clusterVisibility", resources.GetBuffer(clusterVisibilityBuffer));

		struct ClusterData
		{
			int32_t gridDimensionsX;
			int32_t gridDimensionsY;
			int32_t gridDimensionsZ;
			float logY;
		} clusterData;

		clusterData.gridDimensionsX = gridInfo.x;
		clusterData.gridDimensionsY = gridInfo.y;
		clusterData.gridDimensionsZ = gridInfo.z;
		clusterData.logY = 1.f / std::log(gridInfo.depthFactor);

		std::vector<uint32_t> clusterConstants;
		clusterConstants.resize(sizeof(ClusterData) / 4);
		std::memcpy(clusterConstants.data(), &clusterData, clusterConstants.size() * sizeof(uint32_t));

		list.BindConstants("clusterData", clusterConstants);

		list.DrawFullscreenQuad();
	});

	return clusterDebugOverlayTag;
}