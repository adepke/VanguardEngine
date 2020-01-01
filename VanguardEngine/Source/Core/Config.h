// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <filesystem>

static std::filesystem::path EngineConfigPath{ "Config/Engine.json" };

class Config
{
public:
	std::filesystem::path EngineRoot;
	std::filesystem::path ShaderPath;

public:
	static inline Config& Get() noexcept
	{
		static Config Singleton;
		return Singleton;
	}

	Config();
	Config(const Config&) = delete;
	Config(Config&&) noexcept = delete;

	Config& operator=(const Config&) = delete;
	Config& operator=(Config&&) noexcept = delete;
};