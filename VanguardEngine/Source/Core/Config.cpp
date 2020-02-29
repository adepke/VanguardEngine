// Copyright (c) 2019-2020 Andrew Depke

#include <Core/Config.h>
#include <Core/Base.h>

#include <fstream>

#include <json.hpp>

Config::Config()
{
	VGScopedCPUStat("Config Load");

	auto CurrentPath = std::filesystem::current_path();

	constexpr auto IsEngineRoot = [](const auto& Path)
	{
		return std::filesystem::exists(Path / EngineConfigPath);
	};

	if (IsEngineRoot(CurrentPath))
	{
		EngineRoot = CurrentPath;
	}

	else if (IsEngineRoot(CurrentPath.parent_path().parent_path() / "VanguardEngine"))
	{
		EngineRoot = CurrentPath.parent_path().parent_path() / "VanguardEngine";
	}

	else
	{
		VGLogFatal(Core) << "Failed to find engine root.";
	}

	std::ifstream EngineConfigStream{ EngineRoot / EngineConfigPath };
	
	VGEnsure(EngineConfigStream.is_open(), "Failed to open engine config file.");
	
	nlohmann::json EngineConfig;
	EngineConfigStream >> EngineConfig;

	MaterialsPath = EngineRoot / EngineConfig["Assets"]["Materials"].get<std::string>();
}