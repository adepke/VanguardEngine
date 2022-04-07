// Copyright (c) 2019-2022 Andrew Depke

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

namespace std
{
	template <>
	struct hash<ShaderMacro>
	{
		size_t operator()(const ShaderMacro& description) const
		{
			return hash<decltype(description.macro)>{}(description.macro);
		}
	};
}