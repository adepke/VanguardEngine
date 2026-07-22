// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/RenderPipeline.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/ResourceHandle.h>

class RenderDevice;
class RenderGraph;
class CommandList;
class Atmosphere;
struct AccelerationStructureResources;

struct CloudResources
{
	RenderResource cloudsScatteringTransmittance;
	RenderResource cloudsDepth;
	RenderResource cloudsVisibilityMap;
	RenderResource cloudsCirrus;
	RenderResource weather;
};

class Clouds
{
public:
	float coverage = 0.5f;
	float precipitation = 0.3f;
	float windStrength = 0.2f;
	XMFLOAT2 windDirection = { 1, 0 };
	float densityMultiplier = 1.3f;
	int msOctaves = 5;  // Number of multiple-scattering octaves

private:
	RenderDevice* device;

	bool dirty = true;

	static const int weatherSize = 1024;
	static_assert(weatherSize % 8 == 0, "Weather size must be evenly divisible by 8.");

	static const int environmentCloudsSize = 256;
	static_assert(environmentCloudsSize % 8 == 0, "Environment clouds size must be evenly divisible by 8.");
	// Exponential moving average weighting. Lower values smooth the env map updates.
	static constexpr float environmentBlendFactor = 0.15f;

	RenderPipelineLayout weatherLayout;
	RenderPipelineLayout baseNoiseLayout;
	RenderPipelineLayout detailNoiseLayout;
	RenderPipelineLayout curlNoiseLayout;
	RenderPipelineLayout environmentBakeLayout;
	RenderPipelineLayout environmentCompositeLayout;  // Composites the cube over the IBL sky luminance.

	TextureHandle weather;  // 2D, channels: coverage, type, precipitation.
	// Schneider separates density noise into FBM components and composes them while
	// raymarching, but we can merge them here to reduce memory bandwidth at no fidelty
	// loss (see frostbite slides).
	TextureHandle baseShapeNoise;  // 3D, single channel.
	TextureHandle detailShapeNoise;  // 3D, single channel.
	TextureHandle curlShapeNoise;  // 3D, 4 channel (3 in use).

	// Cirrus clouds are not raymarched, they come from a painted texture.
	TextureHandle cirrusClouds;

	// Cubemap of temporally accumulated clouds at the camera location, for IBL and reflections.
	TextureHandle environmentClouds;  // 3D, 4 channel.
	// Tracks accumulation frames, set to 0 for a full re-bake of the cube.
	uint32_t environmentBakeCounter = 0;

	RenderResource lastFrameScatteringUpscaled;
	RenderResource lastFrameDepthUpscaled;
	RenderResource lastFrameVisibilityUpscaled;

	void GenerateWeather(CommandList& list, uint32_t weatherTexture);
	void GenerateNoise(CommandList& list, uint32_t baseShapeTexture, uint32_t detailShapeTexture, uint32_t curlShapeTexture);

public:
	~Clouds();

	void Initialize(RenderDevice* inDevice);
	// tlasTag enables ray traced geometry occlusion in the sky visibility march when available.
	CloudResources Render(RenderGraph& graph, entt::registry& registry, const Atmosphere& atmosphere, const RenderResource cameraBuffer, const RenderResource depthStencil, const RenderResource atmosphereIrradiance, const RenderResource luminanceTag, const AccelerationStructureResources& asResources);
};