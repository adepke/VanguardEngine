// Copyright (c) 2019-2020 Andrew Depke

#include <Core/Config.h>
#include <Core/Base.h>

#include <json.hpp>

#include <fstream>

namespace Config
{
	void Initialize()
	{
		VGScopedCPUStat("Config Load");

		auto CurrentPath = std::filesystem::current_path();

		constexpr auto IsEngineRoot = [](const auto& Path)
		{
			return std::filesystem::exists(Path / EngineConfigPath);
		};

		if (IsEngineRoot(CurrentPath))
		{
			EngineRootPath = CurrentPath;
		}

		// Running from visual studio sandbox.
		else if (IsEngineRoot(CurrentPath.parent_path().parent_path() / "VanguardEngine"))
		{
			EngineRootPath = CurrentPath.parent_path().parent_path() / "VanguardEngine";
		}

		// Running the binary outside of visual studio.
		else if (IsEngineRoot(CurrentPath.parent_path().parent_path().parent_path() / "VanguardEngine"))
		{
			EngineRootPath = CurrentPath.parent_path().parent_path().parent_path() / "VanguardEngine";
		}

		else
		{
			VGLogFatal(Core) << "Failed to find engine root.";
		}

		std::ifstream EngineConfigStream{ EngineRootPath / EngineConfigPath };

		VGEnsure(EngineConfigStream.is_open(), "Failed to open engine config file.");

		nlohmann::json EngineConfig;
		EngineConfigStream >> EngineConfig;

		ShadersPath = EngineRootPath / "Assets" / EngineConfig["ShadersPath"].get<std::string>();
		MaterialsPath = EngineRootPath / "Assets" / EngineConfig["MaterialsPath"].get<std::string>();
	}
}