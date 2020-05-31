// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderComponents.h>

#include <filesystem>

class RenderDevice;

namespace AssetLoader
{
	MeshComponent LoadMesh(RenderDevice& Device, std::filesystem::path Path);
}