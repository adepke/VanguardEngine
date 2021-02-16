// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/Shader.h>

#include <filesystem>
#include <map>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;

struct PipelineStateDescription
{
	std::filesystem::path shaderPath;
	D3D12_BLEND_DESC blendDescription;
	D3D12_RASTERIZER_DESC rasterizerDescription;
	D3D12_DEPTH_STENCIL_DESC depthStencilDescription;
	D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
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
	PipelineStateDescription description;
	PipelineStateReflection reflection;

	void ReflectRootSignature();

	void CreateShaders(RenderDevice& device, const std::filesystem::path& shaderPath);
	void CreateRootSignature(RenderDevice& device);
	void CreateDescriptorTables(RenderDevice& device);

public:
	ResourcePtr<ID3D12RootSignature> rootSignature;
	std::unique_ptr<Shader> vertexShader;
	std::unique_ptr<Shader> pixelShader;
	std::unique_ptr<Shader> hullShader;
	std::unique_ptr<Shader> domainShader;
	std::unique_ptr<Shader> geometryShader;

	auto* Native() const noexcept { return pipeline.Get(); }

	auto* GetReflectionData() const noexcept { return &reflection; }

	void Build(RenderDevice& device, const PipelineStateDescription& inDescription, bool backBufferOutput);
};