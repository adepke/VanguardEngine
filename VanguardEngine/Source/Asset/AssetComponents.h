// Copyright (c) 2019-2022 Andrew Depke

#pragma once

// #TODO: Huge header leakage for serialization, json_fwd doesn't help here either.
#include <json.hpp>

#include <filesystem>

// Stores a relative and absolute path reference to an asset. When loaded, this will
// result in the corresponding component(s) (e.g. MeshComponent) being constructed.
struct AssetComponent
{
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(AssetComponent, relativePath, absolutePath);

	std::filesystem::path relativePath;
	std::filesystem::path absolutePath = {};  // Prefer relative.
};