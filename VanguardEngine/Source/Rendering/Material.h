// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <memory>

class PipelineState;
struct Texture;

struct Material
{
	std::unique_ptr<PipelineState> Pipeline;

	std::shared_ptr<Texture> Albedo;
	std::shared_ptr<Texture> Normal;
	std::shared_ptr<Texture> Roughness;
	std::shared_ptr<Texture> Metallic;

	bool BackFaceCulling;
};