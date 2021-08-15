// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/Shader.h>

#include <cstring>

#include <Core/Windows/DirectX12Minimal.h>
#include <dxcapi.h>
#include <d3d12shader.h>

// Interface objects exist for the duration of the application. Can be destroyed after initial compilation during release builds if necessary.
static ResourcePtr<IDxcUtils> shaderUtils;
static ResourcePtr<IDxcCompiler3> shaderCompiler;
static ResourcePtr<IDxcIncludeHandler> shaderIncludeHandler;

void ReflectShader(std::unique_ptr<Shader>& inShader, ID3D12ShaderReflection* reflection, const std::wstring& name)
{
	VGScopedCPUStat("Reflect Shader");

	D3D12_SHADER_DESC shaderDesc;
	auto result = reflection->GetDesc(&shaderDesc);
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Shader reflection for '" << name << "' failed internally: " << result;
		return;
	}

	inShader->reflection.inputElements.reserve(shaderDesc.InputParameters);
	for (uint32_t i = 0; i < shaderDesc.InputParameters; ++i)
	{
		D3D12_SIGNATURE_PARAMETER_DESC parameterDesc;

		result = reflection->GetInputParameterDesc(i, &parameterDesc);
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Shader reflection for '" << name << "' failed internally: " << result;
			return;
		}

		inShader->reflection.inputElements.push_back({ parameterDesc.SemanticName, parameterDesc.SemanticIndex });
	}

	inShader->reflection.resourceBindings.reserve(shaderDesc.BoundResources);
	for (uint32_t i = 0; i < shaderDesc.BoundResources; ++i)
	{
		D3D12_SHADER_INPUT_BIND_DESC bindDesc;
		result = reflection->GetResourceBindingDesc(i, &bindDesc);
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Shader reflection for '" << name << "' failed internally: " << result;
			return;
		}

		ShaderReflection::ResourceBindType type = ShaderReflection::ResourceBindType::Unknown;

		switch (bindDesc.Type)
		{
		case D3D_SIT_CBUFFER: type = ShaderReflection::ResourceBindType::ConstantBuffer; break;
		case D3D_SIT_TBUFFER: type = ShaderReflection::ResourceBindType::ShaderResource; break;
		case D3D_SIT_TEXTURE: type = ShaderReflection::ResourceBindType::ShaderResource; break;
		case D3D_SIT_SAMPLER: break;  // Ignore samplers for now.
		case D3D_SIT_UAV_RWTYPED: type = ShaderReflection::ResourceBindType::UnorderedAccess; break;
		case D3D_SIT_STRUCTURED: type = ShaderReflection::ResourceBindType::ShaderResource; break;
		case D3D_SIT_UAV_RWSTRUCTURED: type = ShaderReflection::ResourceBindType::UnorderedAccess; break;
		case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: type = ShaderReflection::ResourceBindType::UnorderedAccess; break;
		default: VGLogError(Rendering) << "Shader reflection for '" << name << "' failed internally: " << "Unknown resource bind type '" << bindDesc.Type << "'.";
		}

		inShader->reflection.resourceBindings.push_back({ bindDesc.Name, bindDesc.BindPoint, bindDesc.BindCount, bindDesc.Space, type });
	}

	inShader->reflection.instructionCount = shaderDesc.InstructionCount;
}

std::unique_ptr<Shader> CompileShader(const std::filesystem::path& path, ShaderType type, std::string_view entry, const std::vector<ShaderMacro>& macros)
{
	VGScopedCPUStat("Compile Shader");

	VGLog(Rendering) << "Compiling shader: " << path.generic_wstring();

	if (!shaderUtils)
	{
		auto result = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(shaderUtils.Indirect()));
		if (FAILED(result))
		{
			VGLogFatal(Rendering) << "Failed to create DXC utilities: " << result;
		}
	}

	if (!shaderCompiler)
	{
		auto result = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(shaderCompiler.Indirect()));
		if (FAILED(result))
		{
			VGLogFatal(Rendering) << "Failed to create DXC compiler: " << result;
		}
	}

	if (!shaderIncludeHandler)
	{
		auto result = shaderUtils->CreateDefaultIncludeHandler(shaderIncludeHandler.Indirect());
		if (FAILED(result))
		{
			VGLogFatal(Rendering) << "Failed to create DXC include handler: " << result;
		}
	}

	auto* compileTarget = VGText("");

	switch (type)
	{
	case ShaderType::Vertex: compileTarget = VGText("vs_6_6"); break;
	case ShaderType::Pixel: compileTarget = VGText("ps_6_6"); break;
	case ShaderType::Compute: compileTarget = VGText("cs_6_6"); break;
	}

	auto pathModified = path;

	if (!path.has_extension())
	{
		pathModified.replace_extension(".hlsl");
	}

	ResourcePtr<IDxcBlobEncoding> sourceBlob;
	auto result = shaderUtils->LoadFile(pathModified.c_str(), nullptr, sourceBlob.Indirect());
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to create shader blob at '" << path.generic_wstring() << "': " << result;

		return {};
	}
	
	DxcBuffer sourceBuffer;
	sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
	sourceBuffer.Size = sourceBlob->GetBufferSize();
	sourceBuffer.Encoding = DXC_CP_ACP;

	auto includeSearchDirectory = path;
	includeSearchDirectory.remove_filename();

	// Stable names so we don't have to worry about c_str() being overwritten.
	const auto stableShaderName = pathModified.filename().generic_wstring();
	const auto stableShaderIncludePath = includeSearchDirectory.generic_wstring();

	const auto entryWide = std::wstring{ entry.begin(), entry.end() };

	// See https://github.com/microsoft/DirectXShaderCompiler/blob/master/include/dxc/Support/HLSLOptions.td for compiler arguments.

	std::vector<const wchar_t*> compileArguments;
	compileArguments.emplace_back(stableShaderName.data());
	compileArguments.emplace_back(VGText("-E"));
	compileArguments.emplace_back(entryWide.data());
	compileArguments.emplace_back(VGText("-T"));
	compileArguments.emplace_back(compileTarget);
	compileArguments.emplace_back(VGText("-I"));
	compileArguments.emplace_back(stableShaderIncludePath.data());
	//compileArguments.emplace_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);  // Row major matrices. #TODO: Use uniform packing, ImGui uses column major currently.
	compileArguments.emplace_back(VGText("-HV"));  // HLSL version 2018.
	compileArguments.emplace_back(VGText("2018"));
#if BUILD_DEBUG || BUILD_DEVELOPMENT
	compileArguments.emplace_back(DXC_ARG_DEBUG);  // Enable debug information.
	compileArguments.emplace_back(VGText("-Qembed_debug"));  // Embed the PDB.
#endif
#if BUILD_DEBUG
	compileArguments.emplace_back(DXC_ARG_SKIP_OPTIMIZATIONS);  // Disable optimization.
#endif
#if BUILD_DEVELOPMENT
	compileArguments.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL1);  // Basic optimization.
#endif
#if BUILD_RELEASE
	compileArguments.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL3);  // Maximum optimization.
#endif

	// Shader macros.
	std::vector<std::wstring> macrosWide;  // Keep the wide string copies alive until compilation finishes.
	for (const auto& macro : macros)
	{
		macrosWide.emplace_back(std::wstring{ macro.macro.begin(), macro.macro.end() });
	}

	for (const auto& macro : macrosWide)
	{
		compileArguments.emplace_back(VGText("-D"));
		compileArguments.emplace_back(macro.data());
	}

	ResourcePtr<IDxcResult> compileResult;
	shaderCompiler->Compile(
		&sourceBuffer,
		compileArguments.size() ? compileArguments.data() : nullptr,
		static_cast<uint32_t>(compileArguments.size()),
		shaderIncludeHandler.Get(),
		IID_PPV_ARGS(compileResult.Indirect())
	);
	
	ResourcePtr<IDxcBlobUtf8> errorBlob;
	compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorBlob.Indirect()), nullptr);

	compileResult->GetStatus(&result);
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to compile shader at '" << path.generic_wstring() << "': " << result
			<< " | Error: " << ((errorBlob && errorBlob->GetStringLength()) ? errorBlob->GetStringPointer() : "Unknown.");

		return {};
	}

	else if (errorBlob && errorBlob->GetStringLength())
	{
		VGLogWarning(Rendering) << "Compiling shader at '" << path.generic_wstring() << "' had warnings and/or errors: " << errorBlob->GetStringPointer();
	}

	ResourcePtr<IDxcBlob> compiledShader;
	compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(compiledShader.Indirect()), nullptr);
	if (!compiledShader)
	{
		VGLogError(Rendering) << "Failed to get compiled shader object.";

		return {};
	}

	auto resultShader = std::make_unique<Shader>();
	resultShader->bytecode.resize(compiledShader->GetBufferSize());

	std::memcpy(resultShader->bytecode.data(), compiledShader->GetBufferPointer(), compiledShader->GetBufferSize());

	ResourcePtr<IDxcBlob> shaderReflectionBlob;
	compileResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(shaderReflectionBlob.Indirect()), nullptr);
	if (shaderReflectionBlob)
	{
		DxcBuffer reflectionBuffer;
		reflectionBuffer.Ptr = shaderReflectionBlob->GetBufferPointer();
		reflectionBuffer.Size = shaderReflectionBlob->GetBufferSize();
		reflectionBuffer.Encoding = DXC_CP_ACP;

		ResourcePtr<ID3D12ShaderReflection> reflection;
		shaderUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(reflection.Indirect()));

		ReflectShader(resultShader, reflection.Get(), path.filename().generic_wstring());
	}

	else
	{
		VGLogWarning(Rendering) << "Failed to retrieve shader reflection data.";
	}

	return std::move(resultShader);
}