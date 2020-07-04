// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <filesystem>
#include <memory>

struct Texture;
class RenderDevice;

namespace AssetLoader
{
	std::shared_ptr<Texture> LoadTexture(RenderDevice& device, std::filesystem::path path);
}