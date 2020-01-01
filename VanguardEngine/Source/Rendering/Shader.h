// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <vector>
#include <memory>
#include <filesystem>

#include <d3dcommon.h>

struct ShaderReflection
{
	struct ConstantBuffer
	{
		std::string Name;
	};

	struct Resource
	{
		std::string Name;
		size_t BindPoint;
		size_t BindCount;
	};

	std::vector<ConstantBuffer> ConstantBuffers;
	std::vector<Resource> ResourceBindings;
	size_t InstructionCount = 0;
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
	std::vector<uint8_t> Bytecode;
	ShaderReflection Reflection;

	ResourcePtr<ID3DBlob> Blob;  // Stored for proper cleanup only. Do not access.
};

std::unique_ptr<Shader> CompileShader(const std::filesystem::path& Path, ShaderType Type);