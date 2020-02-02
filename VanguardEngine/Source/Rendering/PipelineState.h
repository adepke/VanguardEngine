// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/Shader.h>

#include <filesystem>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;

struct PipelineStateDescription
{
	std::filesystem::path ShaderPath;
	D3D12_BLEND_DESC BlendDescription;
	D3D12_RASTERIZER_DESC RasterizerDescription;
	D3D12_DEPTH_STENCIL_DESC DepthStencilDescription;
	D3D12_PRIMITIVE_TOPOLOGY Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
};

class PipelineState
{
	friend class CommandList;

private:
	ResourcePtr<ID3D12PipelineState> Pipeline;
	PipelineStateDescription Description;

	void CreateShaders(RenderDevice& Device, const std::filesystem::path& ShaderPath);
	void CreateRootSignature(RenderDevice& Device);
	void CreateDescriptorTables(RenderDevice& Device);
	void CreateInputLayout();

public:
	size_t Hash = 0;
	ResourcePtr<ID3D12RootSignature> RootSignature;
	D3D12_INPUT_LAYOUT_DESC InputLayout;
	std::unique_ptr<Shader> VertexShader;
	std::unique_ptr<Shader> PixelShader;
	std::unique_ptr<Shader> HullShader;
	std::unique_ptr<Shader> DomainShader;
	std::unique_ptr<Shader> GeometryShader;
	//ResourcePtr<> BlendState;  // #TODO: Blend state.
	//ResourcePtr<> RasterizerState;  // #TODO: Rasterizer state.
	//ResourcePtr<> DepthStencilState;  // #TODO: Depth stencil state.

	auto* Native() const noexcept { return Pipeline.Get(); }

	void Build(RenderDevice& Device, const PipelineStateDescription& InDescription);
};

namespace std
{
	template <>
	struct hash<PipelineState>
	{
		size_t operator()(const PipelineState& Target) const noexcept
		{
			return 0;  // #TODO: PipelineState hashing.
		}
	};
}