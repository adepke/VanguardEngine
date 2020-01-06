// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <memory>
#include <vector>

struct PipelineState;
struct GPUTexture;

// #TODO: Physically based rendering.
struct Material
{
	// #TODO: Support variable pipeline.
	//std::shared_ptr<PipelineState> Pipeline;

	std::shared_ptr<GPUTexture> DiffuseMap;
};