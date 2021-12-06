// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/PipelineState.h>
#include <Rendering/RenderGraphResource.h>

class RenderDevice;
class RenderGraph;
class Atmosphere;

class ImageBasedLighting
{
private:
	static constexpr uint32_t irradianceTextureSize = 32;
	static constexpr uint32_t prefilterTextureSize = 128;  // Resolution of base mip.
	static constexpr uint32_t prefilterLevels = 8;  // Roughness bins, must be equal to lg(prefilterTextureSize).
	static constexpr uint32_t brdfTextureSize = 512;

	static_assert(irradianceTextureSize <= 32, "irradianceTextureSize must be less than or equal to 32.");
	static_assert(prefilterTextureSize % 8 == 0, "prefilterTextureSize must be evenly divisible by 8.");
	static_assert(brdfTextureSize % 8 == 0, "brdfTextureSize must be evenly divisible by 8.");

public:
	TextureHandle irradianceTexture;
	TextureHandle prefilterTexture;
	TextureHandle brdfTexture;

private:
	PipelineState irradiancePrecompute;
	PipelineState prefilterPrecompute;
	PipelineState brdfPrecompute;

	RenderDevice* device = nullptr;
	bool brdfRendered = false;

public:
	~ImageBasedLighting();
	void Initialize(RenderDevice* inDevice);
	void UpdateLuts(RenderGraph& graph, RenderResource luminanceTexture, RenderResource cameraBuffer);
};