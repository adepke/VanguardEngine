// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Shader.h>
#include <Rendering/ShaderMacro.h>

#include <filesystem>
#include <map>
#include <utility>
#include <vector>

class RenderDevice;

struct GraphicsPipelineStateDescription
{
	std::pair<std::filesystem::path, std::string> vertexShader;
	std::pair<std::filesystem::path, std::string> pixelShader;
	std::vector<ShaderMacro> macros;
	D3D12_BLEND_DESC blendDescription;
	D3D12_RASTERIZER_DESC rasterizerDescription;
	D3D12_DEPTH_STENCIL_DESC depthStencilDescription;
	D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

struct ComputePipelineStateDescription
{
	std::pair<std::filesystem::path, std::string> shader;
	std::vector<ShaderMacro> macros;
};

struct PipelineStateReflection
{
	enum class ResourceBindType
	{
		RootConstants,
		ConstantBuffer,
		ShaderResource,
		UnorderedAccess,
	};

	struct ResourceBindMetadata
	{
		ResourceBindType type;
		size_t signatureIndex;
	};

	// Maps shader resource bind names to bind metadata, generated from the compiled
	// shaders and the deserialized root signature.
	std::map<std::string, ResourceBindMetadata> resourceIndexMap;
};

class PipelineState
{
	friend class CommandList;

private:
	ResourcePtr<ID3D12PipelineState> pipeline;
	GraphicsPipelineStateDescription graphicsDescription;
	ComputePipelineStateDescription computeDescription;
	PipelineStateReflection reflection;

	void ReflectRootSignature();

	void CreateShaders(RenderDevice& device, const std::vector<ShaderMacro>& macros);
	void CreateRootSignature(RenderDevice& device);

public:
	ResourcePtr<ID3D12RootSignature> rootSignature;
	std::unique_ptr<Shader> vertexShader;
	std::unique_ptr<Shader> pixelShader;
	std::unique_ptr<Shader> computeShader;

	auto* Native() const noexcept { return pipeline.Get(); }

	auto* GetReflectionData() const noexcept { return &reflection; }

	void Build(RenderDevice& device, const GraphicsPipelineStateDescription& inDescription, bool backBufferOutput);
	void Build(RenderDevice& device, const ComputePipelineStateDescription& inDescription);
};