// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <filesystem>

namespace Config
{
	inline const std::filesystem::path engineConfigPath{ "Config/Engine.json" };

	// Loaded from config. All paths are absolute.

	inline std::filesystem::path engineRootPath;
	inline std::filesystem::path shadersPath;
	inline std::filesystem::path fontsPath;
	inline std::filesystem::path materialsPath;
	inline std::filesystem::path utilitiesPath;

	void Initialize();
};