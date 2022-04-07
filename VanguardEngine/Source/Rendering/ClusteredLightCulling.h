// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/RenderPipeline.h>

#include <entt/entt.hpp>

// #TEMP
struct MeshResources
{
	RenderResource positionTag;
	RenderResource extraTag;
};

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

	RenderPipelineLayout boundsLayout;
	RenderPipelineLayout depthCullLayout;
	RenderPipelineLayout compactionLayout;
	RenderPipelineLayout binningLayout;
	RenderPipelineLayout indirectGenerationLayout;

#if ENABLE_EDITOR
	// Debugging visualizations.
	RenderPipelineLayout debugOverlayLayout;
#endif

	ResourcePtr<ID3D12CommandSignature> binningIndirectSignature;

	ClusterGridInfo ComputeGridInfo(const entt::registry& registry) const;
	// Needs to be called every time the camera resolution or FOV changes.
	void ComputeClusterGrid(CommandList& list, uint32_t cameraBuffer, uint32_t clusterBoundsBuffer) const;

public:
	~ClusteredLightCulling();

	void Initialize(RenderDevice* inDevice);
	const ClusterGridInfo& GetGridInfo() const { return gridInfo; }
	ClusterResources Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil, RenderResource lightsBuffer, RenderResource instanceBuffer, MeshResources meshResources);
	RenderResource RenderDebugOverlay(RenderGraph& graph, RenderResource lightInfoBuffer, RenderResource clusterVisibilityBuffer);

	void MarkDirty() { dirty = true; };
};