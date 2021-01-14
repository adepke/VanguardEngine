// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/Shader.h>

#include <filesystem>

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

class PipelineState
{
	friend class CommandList;

private:
	ResourcePtr<ID3D12PipelineState> pipeline;
	PipelineStateDescription description;

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

	void Build(RenderDevice& device, const PipelineStateDescription& inDescription);
};