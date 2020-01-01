// Copyright (c) 2019 Andrew Depke

#include <Rendering/PipelineState.h>
#include <Rendering/Device.h>

#include <algorithm>
#include <cctype>
#include <optional>

void PipelineState::CreateShaders(RenderDevice& Device, const std::filesystem::path& ShaderPath)
{
	VGScopedCPUStat("Create Shaders");

	auto PathOnly = ShaderPath;
	PathOnly.remove_filename();

	for (auto& Entry : std::filesystem::directory_iterator{ PathOnly })
	{
		Entry.path().filename().replace_extension("");

		if (Entry.path().filename() == ShaderPath.filename())
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
				VGLogError(Rendering) << "Failed to determine shader type for shader " << Entry.path().filename() << ".";
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

	if (FAILED(Device.Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &RootSignatureFeatureData, sizeof(RootSignatureFeatureData))))
	{
		VGLogFatal(Rendering) << "Adapter doesn't support root signature version 1.1";
	}

	constexpr auto RootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |  // We need the input assembler.
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC RootSignatureDesc{};
	RootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	//RootSignatureDesc.Desc_1_1.NumParameters = ? ;  // #TODO: Comes from reflection.
	//RootSignatureDesc.Desc_1_1.pParameters = ? ;  // #TODO: Comes from reflection.
	RootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
	RootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
	RootSignatureDesc.Desc_1_1.Flags = RootSignatureFlags;

	ID3DBlob* RootSignatureBlob;
	ID3DBlob* ErrorBlob;

	auto Result = D3D12SerializeVersionedRootSignature(&RootSignatureDesc, &RootSignatureBlob, &ErrorBlob);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to serialize root signature: " << Result;
	}

	Result = Device.Device->CreateRootSignature(0, RootSignatureBlob->GetBufferPointer(), RootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(RootSignature.Indirect()));
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

void PipelineState::Build(RenderDevice& Device, const std::filesystem::path& ShaderPath)
{
	VGScopedCPUStat("Build Pipeline");

	VGLog(Rendering) << "Building pipeline for shader " << ShaderPath.filename() << ".";

	const auto& Filename = ShaderPath.filename().generic_wstring();

	if (ShaderPath.has_extension())
	{
		VGLogWarning(Rendering) << "Improper shader path " << ShaderPath.filename() << ", do not include extension.";

		auto ShaderPathFixed = ShaderPath;
		ShaderPathFixed.replace_extension("");

		CreateShaders(Device, ShaderPathFixed);
	}

	else
	{
		CreateShaders(Device, ShaderPath);
	}

	CreateRootSignature(Device);
	CreateDescriptorTables(Device);
	CreateInputLayout();
}