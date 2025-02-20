// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/ResourceHandle.h>

#include <filesystem>

class RenderDevice;

namespace AssetLoader
{
	TextureHandle LoadTexture(RenderDevice& device, std::filesystem::path path, bool sRGB);
}