// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <memory>
#include <vector>

class PipelineState;
struct Texture;

// #TODO: Physically based rendering.
struct Material
{
	std::unique_ptr<PipelineState> Pipeline;

	std::shared_ptr<Texture> DiffuseMap;

	bool BackFaceCulling;
};