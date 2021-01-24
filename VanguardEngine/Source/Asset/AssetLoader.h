// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/RenderComponents.h>

#include <filesystem>

class RenderDevice;

namespace AssetLoader
{
	MeshComponent LoadMesh(RenderDevice& device, const std::filesystem::path& path);
}