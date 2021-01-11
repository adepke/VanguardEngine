// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <filesystem>
#include <memory>

class RenderDevice;

namespace AssetLoader
{
	TextureHandle LoadTexture(RenderDevice& device, std::filesystem::path path);
}