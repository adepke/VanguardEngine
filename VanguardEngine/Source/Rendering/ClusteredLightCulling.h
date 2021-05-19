// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/PipelineState.h>

class RenderDevice;
class CommandList;
class RenderGraph;

class ClusteredLightCulling
{
private:
	RenderDevice* device;

	bool dirty = true;
	BufferHandle clusterFrustums;
	BufferHandle clusterAABBs;

	PipelineState viewFrustumsState;
	PipelineState boundsState;

	void ComputeClusterGrid(CommandList& list, const entt::registry& registry, BufferHandle cameraBuffer);  // Needs to be called every time the camera resolution or FOV changes.

public:
	~ClusteredLightCulling();

	void Initialize(RenderDevice* inDevice);
	void Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer);

	void MarkDirty() { dirty = true; };
};