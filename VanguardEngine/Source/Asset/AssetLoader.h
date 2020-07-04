// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderComponents.h>

#include <filesystem>

class RenderDevice;

namespace AssetLoader
{
	MeshComponent LoadMesh(RenderDevice& device, std::filesystem::path path);
}