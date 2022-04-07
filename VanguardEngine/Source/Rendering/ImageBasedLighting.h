// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderPipeline.h>
#include <Rendering/RenderGraphResource.h>

class RenderDevice;
class RenderGraph;
class Atmosphere;

struct IBLResources
{
	RenderResource irradianceTag;
	RenderResource prefilterTag;
	RenderResource brdfTag;
};

class ImageBasedLighting
{
private:
	static constexpr uint32_t irradianceTextureSize = 32;
	static constexpr uint32_t prefilterTextureSize = 128;  // Resolution of base mip.
	static constexpr uint32_t prefilterLevels = 6;  // Roughness bins, must be <= lg(prefilterTextureSize).
	static constexpr uint32_t brdfTextureSize = 512;

	static_assert(irradianceTextureSize % 8 == 0, "irradianceTextureSize must be evenly divisible by 8.");
	static_assert(prefilterTextureSize % 8 == 0, "prefilterTextureSize must be evenly divisible by 8.");
	static_assert(brdfTextureSize % 8 == 0, "brdfTextureSize must be evenly divisible by 8.");

public:
	TextureHandle irradianceTexture;
	TextureHandle prefilterTexture;
	TextureHandle brdfTexture;

private:
	RenderPipelineLayout irradiancePrecomputeLayout;
	RenderPipelineLayout prefilterPrecomputeLayout;
	RenderPipelineLayout brdfPrecomputeLayout;

	RenderDevice* device = nullptr;
	bool brdfRendered = false;

	uint32_t slice = 0;

public:
	~ImageBasedLighting();
	void Initialize(RenderDevice* inDevice);
	IBLResources UpdateLuts(RenderGraph& graph, RenderResource luminanceTexture, RenderResource cameraBuffer);

	uint32_t GetPrefilterLevels() const { return prefilterLevels; }
};