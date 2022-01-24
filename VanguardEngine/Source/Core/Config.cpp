// Copyright (c) 2019-2022 Andrew Depke

#include <Core/Config.h>
#include <Core/Base.h>

#include <json.hpp>

#include <fstream>

namespace Config
{
	void Initialize()
	{
		VGScopedCPUStat("Config Load");

		auto currentPath = std::filesystem::current_path();

		constexpr auto IsEngineRoot = [](const auto& path)
		{
			return std::filesystem::exists(path / engineConfigPath);
		};

		if (IsEngineRoot(currentPath))
		{
			engineRootPath = currentPath;
		}

		// Running from visual studio sandbox.
		else if (IsEngineRoot(currentPath.parent_path().parent_path() / "VanguardEngine"))
		{
			engineRootPath = currentPath.parent_path().parent_path() / "VanguardEngine";
		}

		// Running the binary outside of visual studio.
		else if (IsEngineRoot(currentPath.parent_path().parent_path().parent_path() / "VanguardEngine"))
		{
			engineRootPath = currentPath.parent_path().parent_path().parent_path() / "VanguardEngine";
		}

		else
		{
			VGLogCritical(logCore, "Failed to find engine root.");
		}

		std::ifstream engineConfigStream{ engineRootPath / engineConfigPath };

		VGEnsure(engineConfigStream.is_open(), "Failed to open engine config file.");

		nlohmann::json engineConfig;
		engineConfigStream >> engineConfig;

		shadersPath = engineRootPath / engineConfig["ShadersPath"].get<std::string>();
		fontsPath = engineRootPath / engineConfig["FontsPath"].get<std::string>();
		materialsPath = engineRootPath / engineConfig["MaterialsPath"].get<std::string>();
	}
}