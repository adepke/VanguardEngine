// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <memory>

class PipelineState;
struct Texture;

struct Material
{
	std::unique_ptr<PipelineState> pipeline;

	std::shared_ptr<Texture> albedo;
	std::shared_ptr<Texture> normal;
	std::shared_ptr<Texture> roughness;
	std::shared_ptr<Texture> metallic;

	bool backFaceCulling;
};