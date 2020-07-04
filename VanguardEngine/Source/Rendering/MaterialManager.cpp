// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/MaterialManager.h>
#include <Core/Config.h>
#include <Rendering/PipelineState.h>

#include <json.hpp>

#include <filesystem>
#include <fstream>

std::vector<Material> MaterialManager::ReloadMaterials(RenderDevice& device)
{
	VGScopedCPUStat("Reload Materials");

	VGLog(Rendering) << "Reloading materials.";

	std::vector<Material> materials;

	for (const auto& entry : std::filesystem::directory_iterator{ Config::materialsPath })
	{
		// #TODO: Move to standardized asset loading pipeline.

		std::ifstream materialStream{ entry };

		if (!materialStream.is_open())
		{
			VGLogError(Rendering) << "Failed to open material asset at '" << entry.path().generic_wstring() << "'.";
			continue;
		}

		nlohmann::json materialData;
		materialStream >> materialData;

		Material newMat{};

		auto materialShaders = materialData["Shaders"].get<std::string>();
		newMat.backFaceCulling = materialData["BackFaceCulling"].get<bool>();

		PipelineStateDescription desc{};
		desc.shaderPath = Config::shadersPath / materialShaders;
		desc.blendDescription.AlphaToCoverageEnable = false;
		desc.blendDescription.IndependentBlendEnable = false;
		desc.blendDescription.RenderTarget[0].BlendEnable = false;
		desc.blendDescription.RenderTarget[0].LogicOpEnable = false;
		desc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		desc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		desc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		desc.blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		desc.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;  // #TODO: Support wire frame rendering.
		desc.rasterizerDescription.CullMode = newMat.backFaceCulling ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
		desc.rasterizerDescription.FrontCounterClockwise = false;
		desc.rasterizerDescription.DepthBias = 0;
		desc.rasterizerDescription.DepthBiasClamp = 0.f;
		desc.rasterizerDescription.SlopeScaledDepthBias = 0.f;
		desc.rasterizerDescription.DepthClipEnable = true;
		desc.rasterizerDescription.MultisampleEnable = false;  // #TODO: Support multi-sampling.
		desc.rasterizerDescription.AntialiasedLineEnable = false;  // #TODO: Support anti-aliasing.
		desc.rasterizerDescription.ForcedSampleCount = 0;
		desc.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		desc.depthStencilDescription.DepthEnable = true;
		desc.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilDescription.StencilEnable = false;  // #TODO: Support stencil.
		desc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		desc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		desc.depthStencilDescription.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.depthStencilDescription.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.depthStencilDescription.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.depthStencilDescription.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.depthStencilDescription.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.depthStencilDescription.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		newMat.pipeline = std::make_unique<PipelineState>();
		newMat.pipeline->Build(device, desc);  // #TODO: Pipeline libraries.

		materials.push_back(std::move(newMat));
	}

	return std::move(materials);
}