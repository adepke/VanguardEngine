// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Material.h>

#include <filesystem>
#include <memory>

class TextureLoader
{
public:
	static std::shared_ptr<Texture> Load(std::filesystem::path Path);
};