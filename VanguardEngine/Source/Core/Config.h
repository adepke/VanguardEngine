// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <filesystem>

namespace Config
{
	inline const std::filesystem::path engineConfigPath{ "Config/Engine.json" };

	// Loaded from config. All paths are absolute.

	inline std::filesystem::path engineRootPath;
	inline std::filesystem::path shadersPath;
	inline std::filesystem::path materialsPath;

	void Initialize();
};