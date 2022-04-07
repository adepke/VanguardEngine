// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Utility/HashCombine.h>
#include <Rendering/PipelineState.h>

#include <Core/Windows/DirectX12Minimal.h>

namespace std
{
	template <>
	struct hash<D3D12_RENDER_TARGET_BLEND_DESC>
	{
		size_t operator()(const D3D12_RENDER_TARGET_BLEND_DESC& blend) const
		{
			size_t seed = 0;
			HashCombine(seed,
				blend.BlendEnable,
				blend.LogicOpEnable,
				blend.SrcBlend,
				blend.DestBlend,
				blend.BlendOp,
				blend.SrcBlendAlpha,
				blend.DestBlendAlpha,
				blend.BlendOpAlpha,
				blend.LogicOp,
				blend.RenderTargetWriteMask);
			return seed;
		}
	};

	template <>
	struct hash<D3D12_BLEND_DESC>
	{
		size_t operator()(const D3D12_BLEND_DESC& desc) const
		{
			size_t seed = 0;
			HashCombine(seed,
				desc.AlphaToCoverageEnable,
				desc.IndependentBlendEnable,
				desc.RenderTarget[0],
				desc.RenderTarget[1],
				desc.RenderTarget[2],
				desc.RenderTarget[3],
				desc.RenderTarget[4],
				desc.RenderTarget[5],
				desc.RenderTarget[6],
				desc.RenderTarget[7]);
			return seed;
		}
	};

	template <>
	struct hash<D3D12_RASTERIZER_DESC>
	{
		size_t operator()(const D3D12_RASTERIZER_DESC& desc) const
		{
			size_t seed = 0;
			HashCombine(seed,
				desc.FillMode,
				desc.CullMode,
				desc.FrontCounterClockwise,
				desc.DepthBias,
				desc.DepthBiasClamp,
				desc.SlopeScaledDepthBias,
				desc.DepthClipEnable,
				desc.MultisampleEnable,
				desc.AntialiasedLineEnable,
				desc.ForcedSampleCount,
				desc.ConservativeRaster);
			return seed;
		}
	};

	template <>
	struct hash<D3D12_DEPTH_STENCILOP_DESC>
	{
		size_t operator()(const D3D12_DEPTH_STENCILOP_DESC& desc) const
		{
			size_t seed = 0;
			HashCombine(seed,
				desc.StencilFailOp,
				desc.StencilDepthFailOp,
				desc.StencilPassOp,
				desc.StencilFunc);
			return seed;
		}
	};

	template <>
	struct hash<D3D12_DEPTH_STENCIL_DESC>
	{
		size_t operator()(const D3D12_DEPTH_STENCIL_DESC& desc) const
		{
			size_t seed = 0;
			HashCombine(seed,
				desc.DepthEnable,
				desc.DepthWriteMask,
				desc.DepthFunc,
				desc.StencilEnable,
				desc.StencilReadMask,
				desc.StencilWriteMask,
				desc.FrontFace,
				desc.BackFace);
			return seed;
		}
	};

	template <>
	struct hash<GraphicsPipelineStateDescription>
	{
		size_t operator()(const GraphicsPipelineStateDescription& description) const
		{
			size_t seed = 0;
			HashCombine(seed,
				std::filesystem::hash_value(description.vertexShader.first),
				description.vertexShader.second,
				std::filesystem::hash_value(description.pixelShader.first),
				description.pixelShader.second,
				description.blendDescription,
				description.rasterizerDescription,
				description.depthStencilDescription,
				description.topology,
				description.renderTargetCount,
				description.renderTargetFormats[0],
				description.renderTargetFormats[1],
				description.renderTargetFormats[2],
				description.renderTargetFormats[3],
				description.renderTargetFormats[4],
				description.renderTargetFormats[5],
				description.renderTargetFormats[6],
				description.renderTargetFormats[7],
				description.depthStencilFormat);
			for (const auto& macro : description.macros)
			{
				HashCombine(seed, macro);
			}
			return seed;
		}
	};

	template <>
	struct hash<ComputePipelineStateDescription>
	{
		size_t operator()(const ComputePipelineStateDescription& description) const
		{
			size_t seed = 0;
			HashCombine(seed,
				std::filesystem::hash_value(description.shader.first),
				description.shader.second);
			for (const auto& macro : description.macros)
			{
				HashCombine(seed, macro);
			}
			return seed;
		}
	};
}