// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <entt/entt.hpp>

#include <string>

namespace CvarHelpers
{
	void Checkbox(const entt::hashed_string cvar, const std::string& name);
	void Slider(const entt::hashed_string cvar, const std::string& name, float min, float max);
	void Slider(const entt::hashed_string cvar, const std::string& name, int min, int max);
	void Combo(const entt::hashed_string cvar, const std::string& name, const char* const* items, int itemCount);
};