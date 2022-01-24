// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ShaderMacro.h>

#include <vector>
#include <memory>
#include <filesystem>
#include <string_view>
#include <vector>

struct ShaderReflection
{
	struct InputElement
	{
		std::string semanticName;
		size_t semanticIndex;
	};

	enum class ResourceBindType
	{
		Unknown,
		ConstantBuffer,
		ShaderResource,
		UnorderedAccess,
	};

	struct Resource
	{
		std::string name;
		size_t bindPoint;
		size_t bindCount;
		size_t bindSpace;
		ResourceBindType type;
	};

	std::vector<InputElement> inputElements;
	std::vector<Resource> resourceBindings;
	size_t instructionCount = 0;
};

enum class ShaderType
{
	Vertex,
	Pixel,
	Compute,
};

struct Shader
{
	std::vector<uint8_t> bytecode;
	ShaderReflection reflection;
};

std::unique_ptr<Shader> CompileShader(const std::filesystem::path& path, ShaderType type, std::string_view entry, const std::vector<ShaderMacro>& macros);