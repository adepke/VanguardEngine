// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/ClusteredLightCulling.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/RenderComponents.h>

void ClusteredLightCulling::ComputeClusterGrid(CommandList& list, const entt::registry& registry, BufferHandle cameraBuffer)
{
	// Make sure there's at least one camera.
	if (!registry.size<CameraComponent>())
	{
		return;
	}

	constexpr auto froxelSize = 32;
	constexpr auto groupSize = 8;

	float cameraNearPlane;
	float cameraFarPlane;
	float cameraFOV;
	registry.view<const CameraComponent>().each([&](auto entity, const auto& camera)
	{
		cameraNearPlane = camera.nearPlane;
		cameraFarPlane = camera.farPlane;
		cameraFOV = camera.fieldOfView;
	});

	const auto& backBufferComponent = device->GetResourceManager().Get(device->GetBackBuffer());

	const auto subdivisionsX = static_cast<uint32_t>(std::ceil(backBufferComponent.description.width / (float)froxelSize));
	const auto subdivisionsY = static_cast<uint32_t>(std::ceil(backBufferComponent.description.height / (float)froxelSize));
	const float depthFactor = 1.f + (2.f * std::tan(cameraFOV / 2.f)) / (float)subdivisionsY;
	const auto subdivisionsZ = static_cast<uint32_t>(std::floor(std::log(cameraFarPlane / cameraNearPlane) / std::log(depthFactor)));

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

	clusterData.gridDimensionsX = subdivisionsX;
	clusterData.gridDimensionsY = subdivisionsY;
	clusterData.gridDimensionsZ = subdivisionsZ;
	clusterData.nearK = depthFactor;
	clusterData.resolutionX = backBufferComponent.description.width;
	clusterData.resolutionY = backBufferComponent.description.height;

	std::vector<uint32_t> clusterConstants;
	clusterConstants.resize(sizeof(ClusterData) / 4);
	std::memcpy(clusterConstants.data(), &clusterData, clusterConstants.size() * sizeof(uint32_t));

	uint32_t dispatchX = static_cast<uint32_t>(std::ceil(subdivisionsX / (float)groupSize));
	uint32_t dispatchY = static_cast<uint32_t>(std::ceil(subdivisionsY / (float)groupSize));

	// Compute tile frustums.

	list.BindPipelineState(viewFrustumsState);
	list.BindConstants("clusterData", clusterConstants);
	list.BindResource("clusterFrustums", clusterFrustums);
	list.BindResource("camera", cameraBuffer);
	list.Native()->Dispatch(dispatchX, dispatchY, 1);

	// Compute cluster AABBs.

	list.BindPipelineState(boundsState);
	list.BindConstants("clusterData", clusterConstants);
	list.BindResource("clusterAABBs", clusterAABBs);
	list.BindResource("camera", cameraBuffer);
	list.Native()->Dispatch(dispatchX, dispatchY, subdivisionsZ);

	list.UAVBarrier(clusterFrustums);
	list.UAVBarrier(clusterAABBs);
}

ClusteredLightCulling::~ClusteredLightCulling()
{
	device->GetResourceManager().Destroy(clusterFrustums);
	device->GetResourceManager().Destroy(clusterAABBs);
}

void ClusteredLightCulling::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	constexpr auto maxDivisionsX = 80;
	constexpr auto maxDivisionsY = 60;
	constexpr auto maxDivisionsZ = 160;

	BufferDescription clusterFrustumsDesc{};
	clusterFrustumsDesc.updateRate = ResourceFrequency::Static;
	clusterFrustumsDesc.bindFlags = BindFlag::UnorderedAccess;
	clusterFrustumsDesc.accessFlags = AccessFlag::GPUWrite;
	clusterFrustumsDesc.size = maxDivisionsX * maxDivisionsY;
	clusterFrustumsDesc.stride = 64;

	clusterFrustums = device->GetResourceManager().Create(clusterFrustumsDesc, VGText("Cluster frustums"));

	BufferDescription clusterAABBsDesc{};
	clusterAABBsDesc.updateRate = ResourceFrequency::Static;
	clusterAABBsDesc.bindFlags = BindFlag::UnorderedAccess | BindFlag::ShaderResource;
	clusterAABBsDesc.accessFlags = AccessFlag::GPUWrite;
	clusterAABBsDesc.size = maxDivisionsX * maxDivisionsY * maxDivisionsZ;
	clusterAABBsDesc.stride = 32;

	clusterAABBs = device->GetResourceManager().Create(clusterAABBsDesc, VGText("Cluster AABBs"));

	ComputePipelineStateDescription viewFrustumsStateDesc;
	viewFrustumsStateDesc.shader = { "ClusteredLightCulling_CS.hlsl", "ComputeClusterFrustumsMain" };
	viewFrustumsState.Build(*device, viewFrustumsStateDesc);

	ComputePipelineStateDescription boundsStateDesc;
	boundsStateDesc.shader = { "ClusteredLightCulling_CS.hlsl", "ComputeClusterBoundsMain" };
	boundsState.Build(*device, boundsStateDesc);
}

void ClusteredLightCulling::Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer)
{
	if (dirty)
	{
		auto clusterFrustumsTag = graph.Import(clusterFrustums);
		auto clusterAABBsTag = graph.Import(clusterAABBs);

		auto& computeClusterGridPass = graph.AddPass("Compute Cluster Grid Pass", ExecutionQueue::Compute);
		computeClusterGridPass.Read(cameraBuffer, ResourceBind::CBV);
		computeClusterGridPass.Write(clusterFrustumsTag, ResourceBind::UAV);
		computeClusterGridPass.Write(clusterAABBsTag, ResourceBind::UAV);
		computeClusterGridPass.Bind([&, cameraBuffer](CommandList& list, RenderGraphResourceManager& resources)
		{
			ComputeClusterGrid(list, registry, resources.GetBuffer(cameraBuffer));
		});

		dirty = false;
	}
}