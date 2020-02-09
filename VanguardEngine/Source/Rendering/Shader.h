// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <vector>
#include <memory>
#include <filesystem>

#include <Core/Windows/DirectX12Minimal.h>

struct ShaderReflection
{
	struct InputElement
	{
		std::string SemanticName;
		size_t SemanticIndex;
		DXGI_FORMAT Format;
	};

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

	std::vector<InputElement> InputElements;
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

namespace std
{
	template <>
	struct hash<Shader>
	{
		size_t operator()(const Shader& Target) const noexcept
		{
			// #TEMP
			return 0;

			//return std::hash{ Target.Bytecode }();
		}
	};
}