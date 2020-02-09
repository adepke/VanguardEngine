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

	D3D12_FEATURE_DATA_ROOT_SIGNATURE RootSignatureFeatureData{};
	RootSignatureFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(Device.Native()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &RootSignatureFeatureData, sizeof(RootSignatureFeatureData))))
	{
		VGLogFatal(Rendering) << "Adapter doesn't support root signature version 1.1";
	}

	auto RootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;  // We need the input assembler.

	if (!VertexShader)
	{
		RootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	}

	if (!PixelShader)
	{
		RootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	}

	if (!HullShader)
	{
		RootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
	}

	if (!DomainShader)
	{
		RootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	}

	if (!GeometryShader)
	{
		RootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	}

	std::vector<D3D12_ROOT_PARAMETER1> RootParameters;
	// #TEMP: Fill out root parameters.

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc{};
	RootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	RootSignatureDesc.Desc_1_1.NumParameters = static_cast<UINT>(RootParameters.size());
	RootSignatureDesc.Desc_1_1.pParameters = RootParameters.data();
	RootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
	RootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;  // #TODO: Support static samplers.
	RootSignatureDesc.Desc_1_1.Flags = RootSignatureFlags;

	ID3DBlob* RootSignatureBlob;
	ID3DBlob* ErrorBlob;

	auto Result = D3D12SerializeVersionedRootSignature(&RootSignatureDesc, &RootSignatureBlob, &ErrorBlob);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to serialize root signature: " << Result;
	}

	Result = Device.Native()->CreateRootSignature(0, RootSignatureBlob->GetBufferPointer(), RootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(RootSignature.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create root signature: " << Result;
	}
}

void PipelineState::CreateDescriptorTables(RenderDevice& Device)
{
	VGScopedCPUStat("Create Descriptor Tables");
}

void PipelineState::CreateInputLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& InputElements)
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
		InputDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		InputDesc.InstanceDataStepRate = 0;

		InputElements.push_back(std::move(InputDesc));
	}

	InputLayout.pInputElementDescs = InputElements.data();
	InputLayout.NumElements = static_cast<UINT>(InputElements.size());
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
	CreateInputLayout(InputElements);

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