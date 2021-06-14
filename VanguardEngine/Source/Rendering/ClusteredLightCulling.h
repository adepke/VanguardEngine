// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/PipelineState.h>

#include <entt/entt.hpp>

class RenderDevice;
class CommandList;
class RenderGraph;

struct ClusterGridInfo
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	float depthFactor;
};

class ClusteredLightCulling
{
private:
	static constexpr int froxelSize = 32;

private:
	RenderDevice* device;

	bool dirty = true;
	BufferHandle clusterFrustums;
	BufferHandle clusterAABBs;

	PipelineState viewFrustumsState;
	PipelineState boundsState;
	PipelineState depthCullState;
	PipelineState compactionState;

	ClusterGridInfo ComputeGridInfo(const entt::registry& registry) const;
	// Needs to be called every time the camera resolution or FOV changes.
	void ComputeClusterGrid(CommandList& list, const ClusterGridInfo& dimensions, BufferHandle cameraBuffer);

public:
	~ClusteredLightCulling();

	void Initialize(RenderDevice* inDevice);
	void Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil, BufferHandle instanceBuffer, size_t instanceOffset);

	void MarkDirty() { dirty = true; };
};