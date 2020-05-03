// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/Shader.h>

#include <cstring>

#include <d3dcompiler.h>
#include <d3d12shader.h>

void ReflectShader(std::unique_ptr<Shader>& InShader, ID3DBlob* Blob, const std::wstring& Name)
{
	VGScopedCPUStat("Reflect Shader");

	ResourcePtr<ID3D12ShaderReflection> ShaderReflection;

	auto Result = D3DReflect(Blob->GetBufferPointer(), Blob->GetBufferSize(), IID_PPV_ARGS(ShaderReflection.Indirect()));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reflect shader '" << Name << "': " << Result;
		return;
	}

	D3D12_SHADER_DESC ShaderDesc;
	Result = ShaderReflection->GetDesc(&ShaderDesc);
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Shader reflection for '" << Name << "' failed internally: " << Result;
		return;
	}

	InShader->Reflection.InputElements.reserve(ShaderDesc.InputParameters);
	for (uint32_t Index = 0; Index < ShaderDesc.InputParameters; ++Index)
	{
		D3D12_SIGNATURE_PARAMETER_DESC ParameterDesc;

		Result = ShaderReflection->GetInputParameterDesc(Index, &ParameterDesc);
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Shader reflection for '" << Name << "' failed internally: " << Result;
			return;
		}

		InShader->Reflection.InputElements.push_back({ ParameterDesc.SemanticName, ParameterDesc.SemanticIndex });
	}

	InShader->Reflection.ConstantBuffers.reserve(ShaderDesc.ConstantBuffers);
	for (uint32_t Index = 0; Index < ShaderDesc.ConstantBuffers; ++Index)
	{
		auto* ConstantBufferReflection = ShaderReflection->GetConstantBufferByIndex(Index);
		D3D12_SHADER_BUFFER_DESC BufferDesc;
		Result = ConstantBufferReflection->GetDesc(&BufferDesc);
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Shader reflection for '" << Name << "' failed internally: " << Result;
			return;
		}

		InShader->Reflection.ConstantBuffers.push_back({ BufferDesc.Name });
	}

	InShader->Reflection.ResourceBindings.reserve(ShaderDesc.BoundResources);
	for (uint32_t Index = 0; Index < ShaderDesc.BoundResources; ++Index)
	{
		D3D12_SHADER_INPUT_BIND_DESC BindDesc;
		Result = ShaderReflection->GetResourceBindingDesc(Index, &BindDesc);
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Shader reflection for '" << Name << "' failed internally: " << Result;
			return;
		}

		InShader->Reflection.ResourceBindings.push_back({ BindDesc.Name, BindDesc.BindPoint, BindDesc.BindCount });
	}

	InShader->Reflection.InstructionCount = ShaderDesc.InstructionCount;
}

std::unique_ptr<Shader> CompileShader(const std::filesystem::path& Path, ShaderType Type)
{
	VGScopedCPUStat("Compile Shader");

	VGLog(Rendering) << "Compiling shader: " << Path.generic_wstring();

	auto* CompileTarget = "";

	switch (Type)
	{
	case ShaderType::Vertex: CompileTarget = "vs_5_0"; break;
	case ShaderType::Pixel: CompileTarget = "ps_5_0"; break;
	case ShaderType::Hull: CompileTarget = "hs_5_0"; break;
	case ShaderType::Domain: CompileTarget = "ds_5_0"; break;
	case ShaderType::Geometry: CompileTarget = "gs_5_0"; break;
	case ShaderType::Compute: CompileTarget = "cs_5_0"; break;
	}

	ResourcePtr<ID3DBlob> Blob;
	ResourcePtr<ID3DBlob> ErrorBlob;

	auto Flags = 0;

#if BUILD_DEBUG
	Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	auto Result = D3DCompileFromFile(Path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", CompileTarget, Flags, 0, Blob.Indirect(), ErrorBlob.Indirect());
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to compile shader at '" << Path.generic_wstring() << "': " << Result << " | Error Blob: " << static_cast<char*>(ErrorBlob->GetBufferPointer());
		return {};
	}

	auto ResultShader{ std::make_unique<Shader>() };
	ResultShader->Bytecode.resize(Blob->GetBufferSize());

	std::memcpy(ResultShader->Bytecode.data(), Blob->GetBufferPointer(), Blob->GetBufferSize());
	
	ReflectShader(ResultShader, Blob.Get(), Path.filename().generic_wstring());

	ResultShader->SetBlob(std::move(Blob));

	return std::move(ResultShader);
}