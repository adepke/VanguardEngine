// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/MaterialManager.h>
#include <Core/Config.h>
#include <Rendering/PipelineState.h>

#include <json.hpp>

#include <filesystem>
#include <fstream>

#include <Core/Windows/WindowsMinimal.h>

MaterialManager::MaterialManager()
{
	if (CompilerModule = ::LoadLibrary(VGText("dxcompiler.dll")); !CompilerModule)
	{
		VGLogFatal(Rendering) << "Failed to load DirectX shader compiler DLL: " << GetPlatformError();
	}
}

MaterialManager::~MaterialManager()
{
	if (CompilerModule)
	{
		if (!::FreeLibrary(static_cast<HMODULE>(CompilerModule)))
		{
			VGLogError(Rendering) << "Failed to free DirectX shader compiler DLL: " << GetPlatformError();
		}
	}
}

std::vector<Material> MaterialManager::ReloadMaterials(RenderDevice& Device)
{
	VGScopedCPUStat("Reload Materials");

	VGLog(Rendering) << "Reloading materials.";

	std::vector<Material> Materials;

	for (const auto& Entry : std::filesystem::directory_iterator{ Config::Get().MaterialsPath })
	{
		// #TODO: Move to standardized asset loading pipeline.

		std::ifstream MaterialStream{ Entry };

		if (!MaterialStream.is_open())
		{
			VGLogError(Rendering) << "Failed to open material asset at '" << Entry.path().generic_wstring() << "'.";
			continue;
		}

		nlohmann::json MaterialData;
		MaterialStream >> MaterialData;

		Material NewMat{};

		auto MaterialShaders = MaterialData["Shaders"].get<std::string>();
		NewMat.BackFaceCulling = MaterialData["BackFaceCulling"].get<bool>();

		PipelineStateDescription Desc{};
		Desc.ShaderPath = Config::Get().EngineRoot / MaterialShaders;
		Desc.BlendDescription.AlphaToCoverageEnable = false;
		Desc.BlendDescription.IndependentBlendEnable = false;
		Desc.BlendDescription.RenderTarget[0].BlendEnable = false;
		Desc.BlendDescription.RenderTarget[0].LogicOpEnable = false;
		Desc.BlendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		Desc.BlendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		Desc.BlendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		Desc.BlendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		Desc.BlendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		Desc.BlendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		Desc.BlendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		Desc.BlendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		Desc.RasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;  // #TODO: Support wire frame rendering.
		Desc.RasterizerDescription.CullMode = NewMat.BackFaceCulling ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
		Desc.RasterizerDescription.FrontCounterClockwise = false;
		Desc.RasterizerDescription.DepthBias = 0;
		Desc.RasterizerDescription.DepthBiasClamp = 0.f;
		Desc.RasterizerDescription.SlopeScaledDepthBias = 0.f;
		Desc.RasterizerDescription.DepthClipEnable = true;
		Desc.RasterizerDescription.MultisampleEnable = false;  // #TODO: Support multi-sampling.
		Desc.RasterizerDescription.AntialiasedLineEnable = false;  // #TODO: Support anti-aliasing.
		Desc.RasterizerDescription.ForcedSampleCount = 0;
		Desc.RasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		Desc.DepthStencilDescription.DepthEnable = true;
		Desc.DepthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		Desc.DepthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		Desc.DepthStencilDescription.StencilEnable = false;  // #TODO: Support stencil.
		Desc.DepthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		Desc.DepthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		Desc.DepthStencilDescription.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		Desc.DepthStencilDescription.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		Desc.Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

		NewMat.Pipeline = std::make_unique<PipelineState>();
		NewMat.Pipeline->Build(Device, Desc);  // #TODO: Pipeline libraries.

		Materials.push_back(std::move(NewMat));
	}

	return std::move(Materials);
}