// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/PipelineState.h>
#include <Rendering/Device.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <vector>
#include <limits>

void PipelineState::ReflectRootSignature()
{
	ResourcePtr<ID3D12RootSignatureDeserializer> deserializer;

	const auto& rootSignatureData = vertexShader ? vertexShader->bytecode : computeShader->bytecode;

	const auto result = D3D12CreateRootSignatureDeserializer(rootSignatureData.data(), rootSignatureData.size(), IID_PPV_ARGS(deserializer.Indirect()));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to create root signature deserializer during reflection: " << result;

		return;
	}

	const auto* rootSignatureDescription = deserializer->GetRootSignatureDesc();
	for (int i = 0; i < rootSignatureDescription->NumParameters; ++i)
	{
		const auto& parameter = rootSignatureDescription->pParameters[i];

		size_t shaderRegister = 0;
		size_t shaderSpace = 0;
		PipelineStateReflection::ResourceBindType type;

		switch (parameter.ParameterType)
		{
		case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			shaderRegister = parameter.Constants.ShaderRegister;
			shaderSpace = parameter.Constants.RegisterSpace;
			type = PipelineStateReflection::ResourceBindType::RootConstants;
			break;
		case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_CBV:
			shaderRegister = parameter.Constants.ShaderRegister;
			shaderSpace = parameter.Constants.RegisterSpace;
			type = PipelineStateReflection::ResourceBindType::ConstantBuffer;
			break;
		case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_SRV:
			shaderRegister = parameter.Descriptor.ShaderRegister;
			shaderSpace = parameter.Descriptor.RegisterSpace;
			type = PipelineStateReflection::ResourceBindType::ShaderResource;
			break;
		case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_UAV:
			shaderRegister = parameter.Descriptor.ShaderRegister;
			shaderSpace = parameter.Descriptor.RegisterSpace;
			type = PipelineStateReflection::ResourceBindType::UnorderedAccess;
			break;
		case D3D12_ROOT_PARAMETER_TYPE::D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			// For tables, we only need to match the first descriptor of the first range.
			// We don't care about the rest because when binding, we just bind the descriptor block start.
			const auto& firstDescriptorRange = parameter.DescriptorTable.pDescriptorRanges[0];

			shaderRegister = firstDescriptorRange.BaseShaderRegister;
			shaderSpace = firstDescriptorRange.RegisterSpace;

			switch (firstDescriptorRange.RangeType)
			{
			case D3D12_DESCRIPTOR_RANGE_TYPE_CBV: type = PipelineStateReflection::ResourceBindType::ConstantBuffer; break;
			case D3D12_DESCRIPTOR_RANGE_TYPE_SRV: type = PipelineStateReflection::ResourceBindType::ShaderResource; break;
			case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: type = PipelineStateReflection::ResourceBindType::UnorderedAccess; break;
			}

			break;
		}

		constexpr auto BindTypeMatches = [](const auto shaderReflectionType, const auto rootSignatureReflectionType)
		{
			switch (shaderReflectionType)
			{
			case ShaderReflection::ResourceBindType::ConstantBuffer:
				return
					rootSignatureReflectionType == PipelineStateReflection::ResourceBindType::RootConstants ||
					rootSignatureReflectionType == PipelineStateReflection::ResourceBindType::ConstantBuffer;
			case ShaderReflection::ResourceBindType::ShaderResource:
				return rootSignatureReflectionType == PipelineStateReflection::ResourceBindType::ShaderResource;
			case ShaderReflection::ResourceBindType::UnorderedAccess:
				return rootSignatureReflectionType == PipelineStateReflection::ResourceBindType::UnorderedAccess;
			}

			return false;
		};

		// Match the register and space to the corresponding shader reflection resource bind.
		const auto MatchShaderBindings = [&](const auto& shader)
		{
			bool hasFoundBinding = false;

			if (!shader)
			{
				return;
			}

			for (const auto& binding : shader->reflection.resourceBindings)
			{
				if (binding.bindPoint == shaderRegister && binding.bindSpace == shaderSpace && BindTypeMatches(binding.type, type))
				{
					VGAssert(!hasFoundBinding, "Already found binding for root signature index '%i'.", i);
					hasFoundBinding = true;

					if (reflection.resourceIndexMap.contains(binding.name))
					{
						const auto otherSignatureIndex = reflection.resourceIndexMap[binding.name].signatureIndex;

						VGAssert(otherSignatureIndex == i, "Multiple unique bind candidates found for '%s' during root signature reflection. Candidates: %i, %i", binding.name, otherSignatureIndex, i);
					}

					else
					{
						reflection.resourceIndexMap[binding.name] = { type, static_cast<size_t>(i) };
					}

#if !BUILD_DEBUG
					break;  // Stop iterating after the first binding, only do this in release for validation purposes.
#endif
				}
			}
		};

		switch (parameter.ShaderVisibility)
		{
		case D3D12_SHADER_VISIBILITY_ALL:
			MatchShaderBindings(vertexShader);
			MatchShaderBindings(pixelShader);
			MatchShaderBindings(hullShader);
			MatchShaderBindings(domainShader);
			MatchShaderBindings(geometryShader);
			MatchShaderBindings(computeShader);
			break;
		case D3D12_SHADER_VISIBILITY_VERTEX:
			MatchShaderBindings(vertexShader);
			break;
		case D3D12_SHADER_VISIBILITY_HULL:
			MatchShaderBindings(hullShader);
			break;
		case D3D12_SHADER_VISIBILITY_DOMAIN:
			MatchShaderBindings(domainShader);
			break;
		case D3D12_SHADER_VISIBILITY_GEOMETRY:
			MatchShaderBindings(geometryShader);
			break;
		case D3D12_SHADER_VISIBILITY_PIXEL:
			MatchShaderBindings(pixelShader);
			break;
		}
	}
}

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

			else if (findType(VGText("_CS")))
			{
				type = ShaderType::Compute;
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
				case ShaderType::Compute:
					computeShader = std::move(compiledShader);
				}
			}
		}
	}
}

void PipelineState::CreateRootSignature(RenderDevice& device)
{
	VGScopedCPUStat("Create Root Signature");

	const auto& rootSignatureData = vertexShader ? vertexShader->bytecode : computeShader->bytecode;

	const auto result = device.Native()->CreateRootSignature(0, rootSignatureData.data(), rootSignatureData.size(), IID_PPV_ARGS(rootSignature.Indirect()));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to create root signature: " << result;
	}

	ReflectRootSignature();
}

void PipelineState::Build(RenderDevice& device, const GraphicsPipelineStateDescription& inDescription, bool backBufferOutput)
{
	VGScopedCPUStat("Build Pipeline");

	graphicsDescription = inDescription;

	VGLog(Rendering) << "Building graphics pipeline for shader '" << graphicsDescription.shaderPath.filename().generic_wstring() << "'.";

	const auto& filename = graphicsDescription.shaderPath.filename().generic_wstring();

	if (graphicsDescription.shaderPath.has_extension())
	{
		VGLogWarning(Rendering) << "Improper shader path '" << graphicsDescription.shaderPath.filename().generic_wstring() << "', do not include extension.";
	}

	CreateShaders(device, graphicsDescription.shaderPath);

	if (!vertexShader)
	{
		VGLogError(Rendering) << "Missing required vertex shader for graphics pipeline state.";

		return;
	}

	CreateRootSignature(device);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc{};
	graphicsDesc.pRootSignature = rootSignature.Get();
	graphicsDesc.VS = { vertexShader->bytecode.data(), vertexShader->bytecode.size() };
	graphicsDesc.PS = { pixelShader ? pixelShader->bytecode.data() : nullptr, pixelShader ? pixelShader->bytecode.size() : 0 };
	graphicsDesc.DS = { domainShader ? domainShader->bytecode.data() : nullptr, domainShader ? domainShader->bytecode.size() : 0 };
	graphicsDesc.HS = { hullShader ? hullShader->bytecode.data() : nullptr, hullShader ? hullShader->bytecode.size() : 0 };
	graphicsDesc.GS = { geometryShader ? geometryShader->bytecode.data() : nullptr, geometryShader ? geometryShader->bytecode.size() : 0 };
	graphicsDesc.StreamOutput = { nullptr, 0, nullptr, 0, 0 };  // Don't support GPU out streaming.
	graphicsDesc.BlendState = graphicsDescription.blendDescription;
	graphicsDesc.SampleMask = std::numeric_limits<UINT>::max();
	graphicsDesc.RasterizerState = graphicsDescription.rasterizerDescription;
	graphicsDesc.DepthStencilState = graphicsDescription.depthStencilDescription;
	graphicsDesc.InputLayout = { nullptr, 0 };  // We aren't using the input assembler, use programmable vertex pulling.
	graphicsDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;  // Don't support strip topology cuts.
	switch (graphicsDescription.topology)  // #TODO: Support patch topology, which is needed for hull and domain shaders.
	{
	case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED: graphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED; break;
	case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: graphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
	case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
	case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: graphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
	case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: graphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
	}
	graphicsDesc.NumRenderTargets = 1;  // #TODO: Pull from render graph.
	graphicsDesc.RTVFormats[0] = backBufferOutput ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R16G16B16A16_FLOAT;  // #TODO: Pull from render graph.
	graphicsDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsDesc.SampleDesc = { 1, 0 };  // #TODO: Support multi-sampling.
	graphicsDesc.NodeMask = 0;
	graphicsDesc.CachedPSO = { nullptr, 0 };  // #TODO: Pipeline caching.
	graphicsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;  // #TODO: Add debugging flag if we're a software adapter.

	const auto result = device.Native()->CreateGraphicsPipelineState(&graphicsDesc, IID_PPV_ARGS(pipeline.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create graphics pipeline state: " << result;
	}
}

void PipelineState::Build(RenderDevice& device, const ComputePipelineStateDescription& inDescription)
{
	VGScopedCPUStat("Build Pipeline");

	computeDescription = inDescription;

	VGLog(Rendering) << "Building compute pipeline for shader '" << computeDescription.shaderPath.filename().generic_wstring() << "'.";

	const auto& filename = computeDescription.shaderPath.filename().generic_wstring();

	if (computeDescription.shaderPath.has_extension())
	{
		VGLogWarning(Rendering) << "Improper shader path '" << computeDescription.shaderPath.filename().generic_wstring() << "', do not include extension.";
	}

	CreateShaders(device, computeDescription.shaderPath);
	CreateRootSignature(device);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
	computeDesc.pRootSignature = rootSignature.Get();
	computeDesc.CS = { computeShader->bytecode.data(), computeShader->bytecode.size() };
	computeDesc.NodeMask = 0;
	computeDesc.CachedPSO = { nullptr, 0 };  // #TODO: Pipeline caching.
	computeDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;  // #TODO: Add debugging flag if we're a software adapter.

	const auto result = device.Native()->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(pipeline.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create compute pipeline state: " << result;
	}
}