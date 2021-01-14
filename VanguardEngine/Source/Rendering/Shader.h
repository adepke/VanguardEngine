// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <vector>
#include <memory>
#include <filesystem>

struct ShaderReflection
{
	struct InputElement
	{
		std::string semanticName;
		size_t semanticIndex;
	};

	struct ConstantBuffer
	{
		std::string name;
	};

	struct Resource
	{
		std::string name;
		size_t bindPoint;
		size_t bindCount;
	};

	std::vector<InputElement> inputElements;
	std::vector<ConstantBuffer> constantBuffers;
	std::vector<Resource> resourceBindings;
	size_t instructionCount = 0;
};

enum class ShaderType
{
	Vertex,
	Pixel,
	Hull,
	Domain,
	Geometry,
	Compute,
};

struct Shader
{
	std::vector<uint8_t> bytecode;
	ShaderReflection reflection;
};

std::unique_ptr<Shader> CompileShader(const std::filesystem::path& path, ShaderType type);