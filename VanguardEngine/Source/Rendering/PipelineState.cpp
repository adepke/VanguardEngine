// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/PipelineState.h>
#include <Rendering/Device.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <vector>
#include <limits>

void PipelineState::CreateShaders(RenderDevice& Device, const std::filesystem::path& ShaderPath)
{
	VGScopedCPUStat("Create Shaders");

	for (auto& Entry : std::filesystem::directory_iterator{ ShaderPath.parent_path() })
	{
		auto EntryName = Entry.path().filename().replace_extension("").generic_wstring();
		EntryName = EntryName.substr(0, EntryName.size() - 3);  // Remove the shader type extension.

		if (EntryName == ShaderPath.filename())
		{
			const auto Filename = Entry.path().filename().generic_wstring();

			std::optional<ShaderType> Type{};

			constexpr auto FindType = [&Filename](const auto* TypeString) { return Filename.find(TypeString) != std::wstring::npos; };

			if (FindType(VGText("_VS")))
			{
				Type = ShaderType::Vertex;
			}

			else if (FindType(VGText("_PS")))
			{
				Type = ShaderType::Pixel;
			}

			else if (FindType(VGText("_HS")))
			{
				Type = ShaderType::Hull;
			}

			else if (FindType(VGText("_DS")))
			{
				Type = ShaderType::Domain;
			}

			else if (FindType(VGText("_GS")))
			{
				Type = ShaderType::Geometry;
			}

			else if (FindType(VGText("_RS")))
			{
				Type = std::nullopt;  // Root signature, we don't need to do anything.
			}

			else
			{
				VGLogError(Rendering) << "Failed to determine shader type for shader " << Entry.path().filename().generic_wstring() << ".";
			}

			if (Type)
			{
				auto CompiledShader = std::move(CompileShader(Entry.path(), Type.value()));
				switch (Type.value())
				{
				case ShaderType::Vertex:
					VertexShader = std::move(CompiledShader);
					break;
				case ShaderType::Pixel:
					PixelShader = std::move(CompiledShader);
					break;
				case ShaderType::Hull:
					HullShader = std::move(CompiledShader);
					break;
				case ShaderType::Domain:
					DomainShader = std::move(CompiledShader);
					break;
				case ShaderType::Geometry:
					GeometryShader = std::move(CompiledShader);
					break;
				}
			}
		}
	}
}

void PipelineState::CreateRootSignature(RenderDevice& Device)
{
	VGScopedCPUStat("Create Root Signature");

	const auto Result = Device.Native()->CreateRootSignature(0, VertexShader->Bytecode.data(), VertexShader->Bytecode.size(), IID_PPV_ARGS(RootSignature.Indirect()));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to create root signature: " << Result;
	}
}

void PipelineState::CreateDescriptorTables(RenderDevice& Device)
{
	VGScopedCPUStat("Create Descriptor Tables");
}

D3D12_INPUT_LAYOUT_DESC PipelineState::CreateInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& InputElements)
{
	VGScopedCPUStat("Create Input Layout");

	for (auto& Element : VertexShader->Reflection.InputElements)
	{
		D3D12_INPUT_ELEMENT_DESC InputDesc{};
		InputDesc.SemanticName = Element.SemanticName.c_str();
		InputDesc.SemanticIndex = static_cast<UINT>(Element.SemanticIndex);
		InputDesc.Format = Element.Format;
		InputDesc.InputSlot = 0;
		InputDesc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		InputDesc.InputSlotClass = Element.SemanticName.find("INSTANCE") != std::string::npos ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		InputDesc.InstanceDataStepRate = InputDesc.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA ? 0 : 1;

		InputElements.push_back(std::move(InputDesc));
	}

	return { InputElements.size() ? InputElements.data() : nullptr, static_cast<UINT>(InputElements.size()) };
}

void PipelineState::Build(RenderDevice& Device, const PipelineStateDescription& InDescription)
{
	VGScopedCPUStat("Build Pipeline");

	Description = InDescription;

	VGLog(Rendering) << "Building pipeline for shader '" << Description.ShaderPath.filename().generic_wstring() << "'.";

	const auto& Filename = Description.ShaderPath.filename().generic_wstring();

	if (Description.ShaderPath.has_extension())
	{
		VGLogWarning(Rendering) << "Improper shader path '" << Description.ShaderPath.filename().generic_wstring() << "', do not include extension.";
	}

	// Data that must be kept alive until the pipeline is created.
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;

	CreateShaders(Device, Description.ShaderPath);
	CreateRootSignature(Device);
	CreateDescriptorTables(Device);
	auto InputLayout = std::move(CreateInputLayout(InputElements));

	if (!VertexShader)
	{
		VGLogError(Rendering) << "Missing required vertex shader for graphics pipeline state.";

		return;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc{};
	Desc.pRootSignature = RootSignature.Get();
	Desc.VS = { VertexShader->Bytecode.data(), VertexShader->Bytecode.size() };
	Desc.PS = { PixelShader ? PixelShader->Bytecode.data() : nullptr, PixelShader ? PixelShader->Bytecode.size() : 0 };
	Desc.DS = { DomainShader ? DomainShader->Bytecode.data() : nullptr, DomainShader ? DomainShader->Bytecode.size() : 0 };
	Desc.HS = { HullShader ? HullShader->Bytecode.data() : nullptr, HullShader ? HullShader->Bytecode.size() : 0 };
	Desc.GS = { GeometryShader ? GeometryShader->Bytecode.data() : nullptr, GeometryShader ? GeometryShader->Bytecode.size() : 0 };
	Desc.StreamOutput = { nullptr, 0, nullptr, 0, 0 };  // Don't support GPU out streaming.
	Desc.BlendState = Description.BlendDescription;
	Desc.SampleMask = std::numeric_limits<UINT>::max();
	Desc.RasterizerState = Description.RasterizerDescription;
	Desc.DepthStencilState = Description.DepthStencilDescription;
	Desc.InputLayout = InputLayout;
	Desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;  // Don't support strip topology cuts.
	switch (Description.Topology)  // #TODO: Support patch topology, which is needed for hull and domain shaders.
	{
	case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED: Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED; break;
	case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
	case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
	}
	Desc.NumRenderTargets = 1;  // #TODO: Pull from render graph?
	Desc.RTVFormats[0] = /*DXGI_FORMAT_R10G10B10A2_UNORM*/ DXGI_FORMAT_B8G8R8A8_UNORM;
	Desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	Desc.SampleDesc = { 1, 0 };  // #TODO: Support multi-sampling.
	Desc.NodeMask = 0;
	Desc.CachedPSO = { nullptr, 0 };  // #TODO: Pipeline caching.
	Desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;  // #TODO: Add debugging flag if we're a software adapter.

	const auto Result = Device.Native()->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(Pipeline.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create pipeline state: " << Result;
	}
}