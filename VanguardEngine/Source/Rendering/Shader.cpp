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

	InShader->Reflection.ConstantBuffers.reserve(ShaderDesc.ConstantBuffers);
	for (auto Index = 0; Index < ShaderDesc.ConstantBuffers; ++Index)
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
	for (auto Index = 0; Index < ShaderDesc.BoundResources; ++Index)
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

	ID3DBlob* Blob;

	auto Flags = 0;

#if BUILD_DEBUG
	Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	auto Result = D3DCompileFromFile(Path.c_str(), nullptr, nullptr, "main", CompileTarget, Flags, 0, &Blob, nullptr);
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to compile shader at '" << Path.generic_wstring() << "': " << Result;
		return {};
	}

	auto ResultShader{ std::make_unique<Shader>() };
	ResultShader->Bytecode.resize(Blob->GetBufferSize());

	std::memcpy(ResultShader->Bytecode.data(), Blob->GetBufferPointer(), Blob->GetBufferSize());

	ResultShader->Blob = std::move(ResourcePtr<ID3DBlob>{ Blob });
	
	ReflectShader(ResultShader, Blob, Path.filename().generic_wstring());

	return std::move(ResultShader);
}