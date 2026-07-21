// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderGraphResource.h>

class RenderDevice;
class RenderGraph;

// Path traced 1 spp jittered and spatio-temporially denoised. Produces
// a screenspace sun shadow mask.
class RayTracedShadows
{
private:
	RenderDevice* device = nullptr;

	// Ping-ponged accumulation history: (visibility, viewZ).
	TextureHandle history[2];
	uint32_t historyWidth = 0;
	uint32_t historyHeight = 0;
	uint32_t historyIndex = 0;
	bool historyValid = false;

	void EnsureHistory(uint32_t width, uint32_t height);

public:
	~RayTracedShadows();
	void Initialize(RenderDevice* inDevice);

	// Returns the shadow mask.
	RenderResource Render(RenderGraph& graph, RenderResource tlasTag, RenderResource depthTag,
		RenderResource normalTag, RenderResource cameraBufferTag, XMFLOAT3 sunDirection, uint32_t frame);

	void RenderDebug(RenderGraph& graph, RenderResource tlasTag, RenderResource cameraBufferTag,
		RenderResource outputTag, int debugMode);
};
