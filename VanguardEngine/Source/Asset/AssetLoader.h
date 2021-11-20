// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/RenderComponents.h>

#include <filesystem>

class RenderDevice;
class MeshFactory;

namespace AssetLoader
{
	MeshComponent LoadMesh(RenderDevice& device, MeshFactory& factory, const std::filesystem::path& path);
}