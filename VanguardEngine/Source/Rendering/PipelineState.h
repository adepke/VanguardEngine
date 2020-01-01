// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/Shader.h>

#include <filesystem>

#include <d3d12.h>

class RenderDevice;

struct PipelineState
{
private:
	void CreateShaders(RenderDevice& Device, const std::filesystem::path& ShaderPath);
	void CreateRootSignature(RenderDevice& Device);
	void CreateDescriptorTables(RenderDevice& Device);
	void CreateInputLayout();

public:
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
	D3D12_PRIMITIVE_TOPOLOGY_TYPE Topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	void Build(RenderDevice& Device, const std::filesystem::path& ShaderPath);
};