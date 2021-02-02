// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/PipelineState.h>
#include <Rendering/Device.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <vector>
#include <limits>

void PipelineState::CreateShaders(RenderDevice& device, const std::filesystem::path& shaderPath)
{
	VGScopedCPUStat("Create Shaders");

	for (auto& entry : std::filesystem::directory_iterator{ shaderPath.parent_path() })
	{
		auto entryName = entry.path().filename().replace_extension("").generic_wstring();
		entryName = entryName.substr(0, entryName.size() - 3);  // Remove the shader type extension.

		if (entryName == shaderPath.filename())
		{
			const auto filename = entry.path().filename().generic_wstring();

			std::optional<ShaderType> type{};

			constexpr auto findType = [&filename](const auto* typeString) { return filename.find(typeString) != std::wstring::npos; };

			if (findType(VGText("_VS")))
			{
				type = ShaderType::Vertex;
			}

			else if (findType(VGText("_PS")))
			{
				type = ShaderType::Pixel;
			}

			else if (findType(VGText("_HS")))
			{
				type = ShaderType::Hull;
			}

			else if (findType(VGText("_DS")))
			{
				type = ShaderType::Domain;
			}

			else if (findType(VGText("_GS")))
			{
				type = ShaderType::Geometry;
			}

			else if (findType(VGText("_RS")))
			{
				type = std::nullopt;  // Root signature, we don't need to do anything.
			}

			else
			{
				VGLogError(Rendering) << "Failed to determine shader type for shader " << entry.path().filename().generic_wstring() << ".";
			}

			if (type)
			{
				auto compiledShader = std::move(CompileShader(entry.path(), type.value()));
				switch (type.value())
				{
				case ShaderType::Vertex:
					vertexShader = std::move(compiledShader);
					break;
				case ShaderType::Pixel:
					pixelShader = std::move(compiledShader);
					break;
				case ShaderType::Hull:
					hullShader = std::move(compiledShader);
					break;
				case ShaderType::Domain:
					domainShader = std::move(compiledShader);
					break;
				case ShaderType::Geometry:
					geometryShader = std::move(compiledShader);
					break;
				}
			}
		}
	}
}

void PipelineState::CreateRootSignature(RenderDevice& device)
{
	VGScopedCPUStat("Create Root Signature");

	const auto result = device.Native()->CreateRootSignature(0, vertexShader->bytecode.data(), vertexShader->bytecode.size(), IID_PPV_ARGS(rootSignature.Indirect()));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to create root signature: " << result;
	}
}

void PipelineState::CreateDescriptorTables(RenderDevice& device)
{
	VGScopedCPUStat("Create Descriptor Tables");
}

void PipelineState::Build(RenderDevice& device, const PipelineStateDescription& inDescription)
{
	VGScopedCPUStat("Build Pipeline");

	description = inDescription;

	VGLog(Rendering) << "Building pipeline for shader '" << description.shaderPath.filename().generic_wstring() << "'.";

	const auto& filename = description.shaderPath.filename().generic_wstring();

	if (description.shaderPath.has_extension())
	{
		VGLogWarning(Rendering) << "Improper shader path '" << description.shaderPath.filename().generic_wstring() << "', do not include extension.";
	}

	CreateShaders(device, description.shaderPath);

	if (!vertexShader)
	{
		VGLogError(Rendering) << "Missing required vertex shader for graphics pipeline state.";

		return;
	}

	CreateRootSignature(device);
	CreateDescriptorTables(device);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature.Get();
	desc.VS = { vertexShader->bytecode.data(), vertexShader->bytecode.size() };
	desc.PS = { pixelShader ? pixelShader->bytecode.data() : nullptr, pixelShader ? pixelShader->bytecode.size() : 0 };
	desc.DS = { domainShader ? domainShader->bytecode.data() : nullptr, domainShader ? domainShader->bytecode.size() : 0 };
	desc.HS = { hullShader ? hullShader->bytecode.data() : nullptr, hullShader ? hullShader->bytecode.size() : 0 };
	desc.GS = { geometryShader ? geometryShader->bytecode.data() : nullptr, geometryShader ? geometryShader->bytecode.size() : 0 };
	desc.StreamOutput = { nullptr, 0, nullptr, 0, 0 };  // Don't support GPU out streaming.
	desc.BlendState = description.blendDescription;
	desc.SampleMask = std::numeric_limits<UINT>::max();
	desc.RasterizerState = description.rasterizerDescription;
	desc.DepthStencilState = description.depthStencilDescription;
	desc.InputLayout = { nullptr, 0 };  // We aren't using the input assembler, use programmable vertex pulling.
	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;  // Don't support strip topology cuts.
	switch (description.topology)  // #TODO: Support patch topology, which is needed for hull and domain shaders.
	{
	case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED: desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED; break;
	case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
	case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
	}
	desc.NumRenderTargets = 1;  // #TODO: Pull from render graph.
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;  // #TODO: Pull from render graph.
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.SampleDesc = { 1, 0 };  // #TODO: Support multi-sampling.
	desc.NodeMask = 0;
	desc.CachedPSO = { nullptr, 0 };  // #TODO: Pipeline caching.
	desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;  // #TODO: Add debugging flag if we're a software adapter.

	const auto result = device.Native()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pipeline.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create pipeline state: " << result;
	}
}