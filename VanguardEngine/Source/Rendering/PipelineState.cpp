// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/PipelineState.h>
#include <Rendering/Device.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>

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

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc{};
	RootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	//RootSignatureDesc.Desc_1_1.NumParameters = ?;  // #TODO: Comes from reflection.
	//RootSignatureDesc.Desc_1_1.pParameters = ?;  // #TODO: Comes from reflection.
	RootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
	RootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;  // #TODO: Static samplers.
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

void PipelineState::CreateInputLayout()
{
	VGScopedCPUStat("Create Input Layout");
}

void PipelineState::Build(RenderDevice& Device, const PipelineStateDescription& InDescription, ID3D12PipelineLibrary* Library)
{
	VGScopedCPUStat("Build Pipeline");

	Description = InDescription;

	VGLog(Rendering) << "Building pipeline for shader '" << Description.ShaderPath.filename().generic_wstring() << "'.";

	const auto& Filename = Description.ShaderPath.filename().generic_wstring();

	if (Description.ShaderPath.has_extension())
	{
		VGLogWarning(Rendering) << "Improper shader path '" << Description.ShaderPath.filename().generic_wstring() << "', do not include extension.";
	}

	CreateShaders(Device, Description.ShaderPath);
	CreateRootSignature(Device);
	CreateDescriptorTables(Device);
	CreateInputLayout();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc{};

	// #TEMP
	Hash = 0;
	//Hash = std::hash<PipelineState>{ *this }();
	std::wstringstream NameStream;
	NameStream << Hash;
	const auto* NameString = NameStream.str().c_str();

	auto Result = Library->LoadGraphicsPipeline(NameString, &Desc, IID_PPV_ARGS(Pipeline.Indirect()));
	if (Result == E_INVALIDARG)
	{
		VGLog(Rendering) << "Did not find pipeline hash in library, creating new pipeline.";

		D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc{};

		Result = Device.Native()->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(Pipeline.Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create pipeline state: " << Result;
		}

		Result = Library->StorePipeline(NameString, Pipeline.Get());
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to store pipeline in library: " << Result;
		}
	}

	else if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to load pipeline from library: " << Result;
	}

	else
	{
		VGLog(Rendering) << "Loaded pipeline from library.";
	}
}

void PipelineState::Bind(ID3D12GraphicsCommandList* CommandList)
{
	CommandList->IASetPrimitiveTopology(Description.Topology);
	CommandList->SetGraphicsRootSignature(RootSignature.Get());
	//CommandList->SetGraphicsRootDescriptorTable()  // #TODO: Set the descriptor table?
	CommandList->SetPipelineState(Pipeline.Get());
}