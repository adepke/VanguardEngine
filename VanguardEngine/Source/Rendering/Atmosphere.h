// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/RenderPipeline.h>
#include <Rendering/Clouds.h>

#include <entt/entt.hpp>

#include <utility>

class PipelineBuilder;
class RenderDevice;
class RenderGraph;
class CommandList;

struct DensityLayer
{
	float width;
	float exponentialCoefficient;
	float exponentialScale;
	float heightScale;
	// Boundary
	float offset;
	XMFLOAT3 padding;
};

struct AtmosphereData
{
	float radiusBottom;  // Planet center to the start of the atmosphere.
	float radiusTop;
	XMFLOAT2 padding0;

	DensityLayer rayleighDensity;
	XMFLOAT3 rayleighScattering;  // Air molecule scattering, absorption is considered negligible.
	float padding1;

	DensityLayer mieDensity;
	XMFLOAT3 mieScattering;
	float padding2;
	XMFLOAT3 mieExtinction;
	float padding3;

	DensityLayer absorptionDensity;
	XMFLOAT3 absorptionExtinction;
	float padding4;

	XMFLOAT3 surfaceColor;  // Average albedo of the planet surface.
	float padding5;

	XMFLOAT3 solarIrradiance;  // #TODO: Separate sun data out of the atmosphere.
	float padding6;
};

struct AtmosphereResources
{
	RenderResource modelHandle;
	RenderResource transmittanceHandle;
	RenderResource scatteringHandle;
	RenderResource irradianceHandle;
};

class Atmosphere
{
public:
	AtmosphereData model;
	entt::entity sunLight;  // Directional light entity for direct solar illumination.

private:
	RenderDevice* device = nullptr;

	bool dirty = true;  // Needs to recompute LUTs.
	TextureHandle transmittanceTexture;
	TextureHandle scatteringTexture;
	TextureHandle irradianceTexture;

	TextureHandle deltaRayleighTexture;
	TextureHandle deltaMieTexture;
	TextureHandle deltaScatteringDensityTexture;
	TextureHandle deltaIrradianceTexture;

	RenderPipelineLayout transmissionPrecomputeLayout;
	RenderPipelineLayout directIrradiancePrecomputeLayout;
	RenderPipelineLayout singleScatteringPrecomputeLayout;
	RenderPipelineLayout scatteringDensityPrecomputeLayout;
	RenderPipelineLayout indirectIrradiancePrecomputeLayout;
	RenderPipelineLayout multipleScatteringPrecomputeLayout;

	void Precompute(CommandList& list, TextureHandle transmittanceHandle, TextureHandle scatteringHandle, TextureHandle irradianceHandle);

	// Storing the atmosphere model data in root descriptors is too expensive and doesn't leave sufficient space for other data,
	// so cache it in a buffer and pass that around instead.
	BufferHandle modelBuffer;

	RenderPipelineLayout skyAmbientPrecomputeLayout;

	// IBL Prefilter base mip is 128x128, so could probably cut this down to that size with no visible loss?
	static constexpr uint32_t luminanceTextureSize = 256;
	static_assert(luminanceTextureSize % 8 == 0, "luminanceTextureSize must be evenly divisible by 8.");

	TextureHandle luminanceTexture;
	RenderPipelineLayout luminancePrecomputeLayout;

public:
	~Atmosphere();
	void Initialize(RenderDevice* inDevice, entt::registry& registry);

	AtmosphereResources ImportResources(RenderGraph& graph);
	void Render(RenderGraph& graph, Clouds& clouds, AtmosphereResources resourceHandles, CloudResources cloudResources, RenderResource cameraBuffer,
		RenderResource depthStencil, RenderResource outputHDRs, entt::registry& registry);
	std::pair<RenderResource, RenderResource> RenderEnvironmentMap(RenderGraph& graph, AtmosphereResources resourceHandles, RenderResource cameraBuffer,
		entt::registry& registry, float globalWeatherCoverage);
	// Separated from RenderEnvironmentMap so other systems can contribute to the cube before mipmapping.
	void GenerateLuminanceMips(RenderGraph& graph, RenderResource luminanceTag);
	uint32_t GetLuminanceTextureSize() const { return luminanceTextureSize; }
	void MarkModelDirty() { dirty = true; }

	RenderResource RenderVisibilityDebugOverlay(RenderGraph& graph, const RenderResource cloudsVisibilityTag, const RenderResource cameraBufferTag,
		int mode, float shadowRange);
};
