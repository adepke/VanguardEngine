// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Rendering/RenderComponents.h>

#include <filesystem>

class AssetLoader
{
public:
	static MeshComponent Load(std::filesystem::path Path);
};