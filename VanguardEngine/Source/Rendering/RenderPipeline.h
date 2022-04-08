// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/PipelineState.h>
#include <Rendering/ShaderMacro.h>
#include <Rendering/PipelineHash.h>

#include <variant>
#include <filesystem>
#include <utility>
#include <unordered_map>
#include <string>
#include <string_view>

struct BlendMode
{
	D3D12_BLEND srcBlend = D3D12_BLEND_ONE;
	D3D12_BLEND destBlend = D3D12_BLEND_ZERO;
	D3D12_BLEND_OP blendOp = D3D12_BLEND_OP_ADD;
	D3D12_BLEND srcBlendAlpha = D3D12_BLEND_ONE;
	D3D12_BLEND destBlendAlpha = D3D12_BLEND_ZERO;
	D3D12_BLEND_OP blendOpAlpha = D3D12_BLEND_OP_ADD;
};

// Don't provide less function since we have an inverse depth buffer.
enum class DepthTestFunction
{
	Equal,
	Greater,
	GreaterEqual
};

class RenderPipelineLayout
{
	friend class RenderGraph;
	friend struct std::hash<RenderPipelineLayout>;

	using GraphicsDesc = GraphicsPipelineStateDescription;
	using ComputeDesc = ComputePipelineStateDescription;

private:
	std::variant<std::monostate, GraphicsDesc, ComputeDesc> description = std::monostate{};

private:
	void InitDefaultGraphics()
	{
		if (auto* value = std::get_if<GraphicsDesc>(&description); !value)
		{
			auto desc = GraphicsDesc{
				.blendDescription = {
					.AlphaToCoverageEnable = false,
					.IndependentBlendEnable = false
				},
				.rasterizerDescription = {
					.FillMode = D3D12_FILL_MODE_SOLID,
					.CullMode = D3D12_CULL_MODE_BACK,
					.FrontCounterClockwise = false,
					.DepthBias = 0,
					.DepthBiasClamp = 0.f,
					.SlopeScaledDepthBias = 0.f,
					.DepthClipEnable = true,
					.MultisampleEnable = false,
					.AntialiasedLineEnable = false,
					.ForcedSampleCount = 0,
					.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
				},
				.depthStencilDescription = {
					.DepthEnable = true,
					.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
					.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
					.StencilEnable = false,
					.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
					.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
					.FrontFace = {
						.StencilFailOp = D3D12_STENCIL_OP_KEEP,
						.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
						.StencilPassOp = D3D12_STENCIL_OP_KEEP,
						.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
					},
					.BackFace = {
						.StencilFailOp = D3D12_STENCIL_OP_KEEP,
						.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
						.StencilPassOp = D3D12_STENCIL_OP_KEEP,
						.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
					}
				}
			};

			desc.blendDescription.RenderTarget[0] = {
				.BlendEnable = false,
				.LogicOpEnable = false,
				.SrcBlend = D3D12_BLEND_ONE,
				.DestBlend = D3D12_BLEND_ZERO,
				.BlendOp = D3D12_BLEND_OP_ADD,
				.SrcBlendAlpha = D3D12_BLEND_ONE,
				.DestBlendAlpha = D3D12_BLEND_ZERO,
				.BlendOpAlpha = D3D12_BLEND_OP_ADD,
				.LogicOp = D3D12_LOGIC_OP_NOOP,
				.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
			};

			description = desc;
		}
	}

	void InitDefaultCompute()
	{
		if (auto* value = std::get_if<ComputeDesc>(&description); !value)
		{
			description = ComputePipelineStateDescription{};
		}
	}

public:
	RenderPipelineLayout& VertexShader(std::pair<std::filesystem::path, std::string> shader)
	{
		InitDefaultGraphics();
		std::get<GraphicsDesc>(description).vertexShader = shader;
		return *this;
	}
	RenderPipelineLayout& PixelShader(std::pair<std::filesystem::path, std::string> shader)
	{
		InitDefaultGraphics();
		std::get<GraphicsDesc>(description).pixelShader = shader;
		return *this;
	}
	RenderPipelineLayout& ComputeShader(std::pair<std::filesystem::path, std::string> shader)
	{
		InitDefaultCompute();
		std::get<ComputeDesc>(description).shader = shader;
		return *this;
	}
	RenderPipelineLayout& BlendMode(bool enabled, BlendMode mode)
	{
		InitDefaultGraphics();
		auto& desc = std::get<GraphicsDesc>(description).blendDescription;
		desc.RenderTarget[0].BlendEnable = enabled;
		desc.RenderTarget[0].LogicOpEnable = false;
		desc.RenderTarget[0].SrcBlend = mode.srcBlend;
		desc.RenderTarget[0].DestBlend = mode.destBlend;
		desc.RenderTarget[0].BlendOp = mode.blendOp;
		desc.RenderTarget[0].SrcBlendAlpha = mode.srcBlendAlpha;
		desc.RenderTarget[0].DestBlendAlpha = mode.destBlendAlpha;
		desc.RenderTarget[0].BlendOpAlpha = mode.blendOpAlpha;
		desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return *this;
	}
	RenderPipelineLayout& FillMode(D3D12_FILL_MODE mode)
	{
		InitDefaultGraphics();
		std::get<GraphicsDesc>(description).rasterizerDescription.FillMode = mode;
		return *this;
	}
	RenderPipelineLayout& CullMode(D3D12_CULL_MODE mode)
	{
		InitDefaultGraphics();
		std::get<GraphicsDesc>(description).rasterizerDescription.CullMode = mode;
		return *this;
	}
	RenderPipelineLayout& DepthEnabled(bool value, bool write = false, DepthTestFunction function = DepthTestFunction::Greater)
	{
		InitDefaultGraphics();
		std::get<GraphicsDesc>(description).depthStencilDescription.DepthEnable = true;
		std::get<GraphicsDesc>(description).depthStencilDescription.DepthWriteMask = write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		switch (function)
		{
		case DepthTestFunction::Equal: std::get<GraphicsDesc>(description).depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL; break;
		case DepthTestFunction::Greater: std::get<GraphicsDesc>(description).depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_GREATER; break;
		case DepthTestFunction::GreaterEqual: std::get<GraphicsDesc>(description).depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL; break;
		}
		return *this;
	}
	RenderPipelineLayout& StencilEnabled(bool value, bool write, uint8_t mask = D3D12_DEFAULT_STENCIL_READ_MASK)
	{
		InitDefaultGraphics();
		std::get<GraphicsDesc>(description).depthStencilDescription.StencilEnable = true;
		std::get<GraphicsDesc>(description).depthStencilDescription.StencilReadMask = write ? 0 : mask;
		std::get<GraphicsDesc>(description).depthStencilDescription.StencilWriteMask = write ? mask : 0;
		return *this;
	}
	RenderPipelineLayout& Topology(D3D12_PRIMITIVE_TOPOLOGY topology)
	{
		InitDefaultGraphics();
		std::get<GraphicsDesc>(description).topology = topology;
		return *this;
	}
	RenderPipelineLayout& Macro(const ShaderMacro& macro)
	{
		std::visit([macro](auto&& desc)
		{
			using T = std::decay_t<decltype(desc)>;
			if constexpr (!std::is_same_v<T, std::monostate>)
				desc.macros.emplace_back(macro);
			else
				VGAssert(false, "Render pipeline layout macros must be added last.");
		}, description);
		return *this;
	}
};

namespace std
{
	template <>
	struct hash<RenderPipelineLayout>
	{
		size_t operator()(const RenderPipelineLayout& layout) const
		{
			return hash<decltype(RenderPipelineLayout::description)>{}(layout.description);
		}
	};
}