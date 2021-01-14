// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <memory>

class PipelineState;

struct Material
{
	std::unique_ptr<PipelineState> pipeline;

	TextureHandle albedo;
	TextureHandle normal;
	TextureHandle roughness;
	TextureHandle metallic;

	bool backFaceCulling;
};