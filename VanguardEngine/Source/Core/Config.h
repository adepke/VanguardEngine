// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <filesystem>

namespace Config
{
	inline const std::filesystem::path EngineConfigPath{ "Config/Engine.json" };

	// Loaded from config. All paths are absolute.

	inline std::filesystem::path EngineRootPath;
	inline std::filesystem::path ShadersPath;
	inline std::filesystem::path MaterialsPath;

	void Initialize();
};