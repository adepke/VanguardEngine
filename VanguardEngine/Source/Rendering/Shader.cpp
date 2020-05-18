// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/Shader.h>

#include <cstring>

#include <Core/Windows/DirectX12Minimal.h>
#include <dxcapi.h>
#include <d3d12shader.h>

// Interface objects exist for the duration of the application. Can be destroyed after initial compilation during release builds if necessary.
static ResourcePtr<IDxcUtils> ShaderUtils;
static ResourcePtr<IDxcCompiler3> ShaderCompiler;
static ResourcePtr<IDxcIncludeHandler> ShaderIncludeHandler;

void ReflectShader(std::unique_ptr<Shader>& InShader, ID3D12ShaderReflection* Reflection, const std::wstring& Name)
{
	VGScopedCPUStat("Reflect Shader");

	D3D12_SHADER_DESC ShaderDesc;
	auto Result = Reflection->GetDesc(&ShaderDesc);
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Shader reflection for '" << Name << "' failed internally: " << Result;
		return;
	}

	InShader->Reflection.InputElements.reserve(ShaderDesc.InputParameters);
	for (uint32_t Index = 0; Index < ShaderDesc.InputParameters; ++Index)
	{
		D3D12_SIGNATURE_PARAMETER_DESC ParameterDesc;

		Result = Reflection->GetInputParameterDesc(Index, &ParameterDesc);
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
		auto* ConstantBufferReflection = Reflection->GetConstantBufferByIndex(Index);
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
		Result = Reflection->GetResourceBindingDesc(Index, &BindDesc);
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

	if (!ShaderUtils)
	{
		auto Result = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(ShaderUtils.Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create DXC utilities: " << Result;
		}
	}

	if (!ShaderCompiler)
	{
		auto Result = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(ShaderCompiler.Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create DXC compiler: " << Result;
		}
	}

	if (!ShaderIncludeHandler)
	{
		auto Result = ShaderUtils->CreateDefaultIncludeHandler(ShaderIncludeHandler.Indirect());
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create DXC include handler: " << Result;
		}
	}

	auto* CompileTarget = VGText("");

	switch (Type)
	{
	case ShaderType::Vertex: CompileTarget = VGText("vs_6_0"); break;
	case ShaderType::Pixel: CompileTarget = VGText("ps_6_0"); break;
	case ShaderType::Hull: CompileTarget = VGText("hs_6_0"); break;
	case ShaderType::Domain: CompileTarget = VGText("ds_6_0"); break;
	case ShaderType::Geometry: CompileTarget = VGText("gs_6_0"); break;
	case ShaderType::Compute: CompileTarget = VGText("cs_6_0"); break;
	}

	ResourcePtr<IDxcBlobEncoding> SourceBlob;
	auto Result = ShaderUtils->LoadFile(Path.c_str(), nullptr, SourceBlob.Indirect());
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to create shader blob at '" << Path.generic_wstring() << "': " << Result;

		return {};
	}
	
	DxcBuffer SourceBuffer;
	SourceBuffer.Ptr = SourceBlob->GetBufferPointer();
	SourceBuffer.Size = SourceBlob->GetBufferSize();
	SourceBuffer.Encoding = DXC_CP_ACP;

	auto IncludeSearchDirectory = Path;
	IncludeSearchDirectory.remove_filename();

	// Stable names so we don't have to worry about c_str() being overwritten.
	const auto StableShaderName = Path.filename().generic_wstring();
	const auto StableShaderIncludePath = IncludeSearchDirectory.generic_wstring();

	std::vector<const wchar_t*> CompileArguments;
	CompileArguments.emplace_back(StableShaderName.data());
	CompileArguments.emplace_back(VGText("-E"));
	CompileArguments.emplace_back(VGText("main"));
	CompileArguments.emplace_back(VGText("-T"));
	CompileArguments.emplace_back(CompileTarget);
	CompileArguments.emplace_back(VGText("-I"));
	CompileArguments.emplace_back(StableShaderIncludePath.data());
#if BUILD_DEBUG || BUILD_DEVELOPMENT
	CompileArguments.emplace_back(DXC_ARG_SKIP_OPTIMIZATIONS);  // Disable optimization.
	CompileArguments.emplace_back(DXC_ARG_DEBUG);  // Enable debug information.
#endif
#if BUILD_RELEASE
	CompileArguments.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL3);  // Maximum optimization.
#endif

	ResourcePtr<IDxcResult> CompileResult;
	ShaderCompiler->Compile(
		&SourceBuffer,
		CompileArguments.size() ? CompileArguments.data() : nullptr,
		CompileArguments.size(),
		ShaderIncludeHandler.Get(),
		IID_PPV_ARGS(CompileResult.Indirect())
	);
	
	ResourcePtr<IDxcBlobUtf8> ErrorBlob;
	CompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(ErrorBlob.Indirect()), nullptr);

	CompileResult->GetStatus(&Result);
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to compile shader at '" << Path.generic_wstring() << "': " << Result
			<< " | Error: " << ((ErrorBlob && ErrorBlob->GetStringLength()) ? ErrorBlob->GetStringPointer() : "Unknown.");

		return {};
	}

	else if (ErrorBlob && ErrorBlob->GetStringLength())
	{
		VGLogWarning(Rendering) << "Compiling shader at '" << Path.generic_wstring() << "' had warnings and/or errors: " << ErrorBlob->GetStringPointer();
	}

	ResourcePtr<IDxcBlob> CompiledShader;
	CompileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(CompiledShader.Indirect()), nullptr);
	if (!CompiledShader)
	{
		VGLogError(Rendering) << "Failed to get compiled shader object.";

		return {};
	}

	auto ResultShader{ std::make_unique<Shader>() };
	ResultShader->Bytecode.resize(CompiledShader->GetBufferSize());

	std::memcpy(ResultShader->Bytecode.data(), CompiledShader->GetBufferPointer(), CompiledShader->GetBufferSize());

	ResourcePtr<IDxcBlob> ShaderReflectionBlob;
	CompileResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(ShaderReflectionBlob.Indirect()), nullptr);
	if (ShaderReflectionBlob)
	{
		DxcBuffer ReflectionBuffer;
		ReflectionBuffer.Ptr = ShaderReflectionBlob->GetBufferPointer();
		ReflectionBuffer.Size = ShaderReflectionBlob->GetBufferSize();
		ReflectionBuffer.Encoding = DXC_CP_ACP;

		ResourcePtr<ID3D12ShaderReflection> Reflection;
		ShaderUtils->CreateReflection(&ReflectionBuffer, IID_PPV_ARGS(Reflection.Indirect()));

		ReflectShader(ResultShader, Reflection.Get(), Path.filename().generic_wstring());
	}

	else
	{
		VGLogWarning(Rendering) << "Failed to retrieve shader reflection data.";
	}

	return std::move(ResultShader);
}