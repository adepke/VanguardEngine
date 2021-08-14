// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <cstring>
#include <string_view>

struct ShaderMacro
{
	std::string macro;

	ShaderMacro(const std::string_view define)
	{
		macro = std::string{ define };
	}

	template <typename T>
	ShaderMacro(const std::string_view define, T value)
	{
		macro = std::string{ define } + "=" + std::to_string(value);
	}
};