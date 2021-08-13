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

struct ClusterResources
{
	RenderResource lightList;
	RenderResource lightInfo;
	RenderResource lightVisibility;
};

class ClusteredLightCulling
{
public:
	static constexpr int froxelSize = 64;
	static constexpr int maxLightsPerFroxel = 256;  // Can realistically reduce this to save memory.

private:
	RenderDevice* device = nullptr;

	bool dirty = true;
	ClusterGridInfo gridInfo;
	BufferHandle clusterBounds;

	PipelineState boundsState;
	PipelineState depthCullState;
	PipelineState compactionState;
	PipelineState binningState;

#if ENABLE_EDITOR
	// Debugging visualizations.
	PipelineState debugOverlayState;
#endif

	ClusterGridInfo ComputeGridInfo(const entt::registry& registry) const;
	// Needs to be called every time the camera resolution or FOV changes.
	void ComputeClusterGrid(CommandList& list, BufferHandle cameraBuffer, BufferHandle clusterBoundsBuffer) const;

public:
	~ClusteredLightCulling();

	void Initialize(RenderDevice* inDevice);
	const ClusterGridInfo& GetGridInfo() const { return gridInfo; }
	ClusterResources Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil, BufferHandle instanceBuffer, size_t instanceOffset, BufferHandle lights);
	RenderResource RenderDebugOverlay(RenderGraph& graph, RenderResource lightInfoBuffer, RenderResource clusterVisibilityBuffer);

	void MarkDirty() { dirty = true; };
};