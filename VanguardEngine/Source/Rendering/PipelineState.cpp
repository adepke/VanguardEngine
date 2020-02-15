// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/PipelineState.h>
#include <Rendering/Device.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <vector>

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
		InputDesc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;  // #TODO: String search to determine if this is a unique element or not.
		InputDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;  // #TODO: Handle per instance data.
		InputDesc.InstanceDataStepRate = 0;

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
	Desc.StreamOutput;
	Desc.BlendState = Description.BlendDescription;
	Desc.SampleMask;
	Desc.RasterizerState = Description.RasterizerDescription;
	Desc.DepthStencilState = Description.DepthStencilDescription;
	Desc.InputLayout = InputLayout;
	Desc.IBStripCutValue;
	Desc.PrimitiveTopologyType;
	Desc.NumRenderTargets;
	Desc.RTVFormats;
	Desc.DSVFormat;
	Desc.SampleDesc;
	Desc.NodeMask = 0;
	Desc.CachedPSO;
	Desc.Flags;

	const auto Result = Device.Native()->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(Pipeline.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create pipeline state: " << Result;
	}
}