// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Resource.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/PipelineState.h>

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

class Atmosphere
{
private:
	RenderDevice* device = nullptr;

	AtmosphereData model;
	bool dirty = true;  // Needs to recompute LUTs.
	TextureHandle transmittanceTexture;
	TextureHandle scatteringTexture;
	TextureHandle irradianceTexture;

	TextureHandle deltaRayleighTexture;
	TextureHandle deltaMieTexture;
	TextureHandle deltaScatteringDensityTexture;
	TextureHandle deltaIrradianceTexture;

	PipelineState transmissionPrecompute;
	PipelineState directIrradiancePrecompute;
	PipelineState singleScatteringPrecompute;
	PipelineState scatteringDensityPrecompute;
	PipelineState indirectIrradiancePrecompute;
	PipelineState multipleScatteringPrecompute;

	void Precompute(CommandList& list);

public:
	~Atmosphere();
	void Initialize(RenderDevice* inDevice);
	void Render(RenderGraph& graph, PipelineBuilder& pipeline, RenderResource cameraBuffer, RenderResource depthStencil, RenderResource outputHDRs);
};