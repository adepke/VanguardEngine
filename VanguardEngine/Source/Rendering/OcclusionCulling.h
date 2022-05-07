// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/RenderPipeline.h>
#include <Rendering/RenderGraphResource.h>

class RenderDevice;
class RenderGraph;

class OcclusionCulling
{
private:
	RenderDevice* device;
	uint32_t swapCount = 0;
	RenderResource lastFrameHiZ;

	RenderPipelineLayout hiZLayout;

#if ENABLE_EDITOR
	// Debugging visualizations.
	RenderPipelineLayout debugOverlayLayout;
#endif

	// How many total levels we want, not mips to generate.
	uint32_t GetMipLevels(RenderGraph& graph);

public:
	void Initialize(RenderDevice* inDevice);
	RenderResource GetLastFrameHiZ();
	void Render(RenderGraph& graph, bool cameraFrozen, const RenderResource depthStencilTag);
	RenderResource RenderDebugOverlay(RenderGraph& graph, int mipLevel, const RenderResource cameraBufferTag);
};

inline RenderResource OcclusionCulling::GetLastFrameHiZ()
{
	VGAssert(swapCount % 2 == 0, "Occlusion culling Render() called before GetLastFrameHiZ().");
	++swapCount;
	return lastFrameHiZ;
}