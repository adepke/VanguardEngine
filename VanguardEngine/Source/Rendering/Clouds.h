// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/RenderPipeline.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/ResourceHandle.h>

class RenderDevice;
class RenderGraph;
class CommandList;
class Atmosphere;

struct CloudResources
{
	RenderResource cloudsScatteringTransmittance;
	RenderResource cloudsDepth;
	RenderResource cloudsShadowMap;
	RenderResource weather;
};

class Clouds
{
public:
	float coverage = 0.5f;
	float precipitation = 0.3f;
	float windStrength = 0.2f;
	XMFLOAT2 windDirection = { 1, 0 };

private:
	RenderDevice* device;

	bool dirty = true;

	static const int weatherSize = 1024;
	static_assert(weatherSize % 8 == 0, "Weather size must be evenly divisible by 8.");

	RenderPipelineLayout weatherLayout;
	RenderPipelineLayout baseNoiseLayout;
	RenderPipelineLayout detailNoiseLayout;
	RenderPipelineLayout cloudsLayout;

	TextureHandle weather;  // 2D, channels: coverage, type, precipitation.
	// Schneider separates density noise into FBM components and composes them while
	// raymarching, but we can merge them here to reduce memory bandwidth at no fidelty
	// loss (see frostbite slides).
	TextureHandle baseShapeNoise;  // 3D, single channel.
	TextureHandle detailShapeNoise;  // 3D, single channel.

	TextureHandle shadowMap;

	RenderResource lastFrameClouds;

	void GenerateWeather(CommandList& list, uint32_t weatherTexture);
	void GenerateNoise(CommandList& list, uint32_t baseShapeTexture, uint32_t detailShapeTexture);

public:
	~Clouds();

	void Initialize(RenderDevice* inDevice);
	CloudResources Render(RenderGraph& graph, const Atmosphere& atmosphere, const RenderResource hdrSource, const RenderResource cameraBuffer, const RenderResource depthStencil, const RenderResource sunTransmittance);
};