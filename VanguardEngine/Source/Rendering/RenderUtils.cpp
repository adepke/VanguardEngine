// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/RenderUtils.h>
#include <Rendering/Base.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Asset/TextureLoader.h>
#include <Core/Config.h>
#include <Utility/FilterKernel.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderPipeline.h>

#include <vector>
#include <cstring>
#include <cmath>

void RenderUtils::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	ComputePipelineStateDescription clearUAVStateDesc{};
	clearUAVStateDesc.shader = { "ClearUAV.hlsl", "Main" };
	clearUAVState.Build(*device, clearUAVStateDesc);

	// #TODO: Generate blue noise instead of loading from a file.
	// #TODO: Fix format, we only need single channel, probably 16 bit precision.
	blueNoise = AssetLoader::LoadTexture(*device, Config::utilitiesPath / "BlueNoise128.png", false);
}

void RenderUtils::Destroy()
{
	device->GetResourceManager().Destroy(blueNoise);
}

void RenderUtils::ClearUAV(CommandList& list, BufferHandle buffer, uint32_t bufferHandle, const DescriptorHandle& nonVisibleDescriptor)
{
	VGScopedGPUStat("Clear UAV", device->GetDirectContext(), list.Native());

	auto& bufferComponent = device->GetResourceManager().Get(buffer);

	// Only non-structured buffers can benefit from hardware fast path for clears.
	if (bufferComponent.description.format)
	{
		constexpr uint32_t clearValues[4] = { 0 };
		// Pass the shader-visible descriptor in as the GPU handle, but the non-visible descriptor for the CPU handle.
		list.Native()->ClearUnorderedAccessViewUint(*bufferComponent.UAV, nonVisibleDescriptor, bufferComponent.Native(), clearValues, 0, nullptr);
	}

	else
	{
		list.BindPipelineState(clearUAVState);

		struct BindData
		{
			uint32_t bufferHandle;
			uint32_t bufferSize;
		} bindData;
		bindData.bufferHandle = bufferHandle;
		bindData.bufferSize = bufferComponent.description.size;

		list.BindConstants("bindData", bindData);

		uint32_t dispatchSize = static_cast<uint32_t>(std::ceil(bufferComponent.description.size / 64.f));
		list.Dispatch(dispatchSize, 1, 1);
	}
}

void RenderUtils::GaussianBlur(CommandList& list, RenderPassResources& resources, RenderResource inputTexture, RenderResource outputTexture, uint32_t radius, float sigma)
{
	if (sigma < 0.f)
	{
		sigma = float(radius) / 2.f;
	}

	const auto weights = GaussianKernel(radius, sigma);
	const int packedWeightSize = std::ceil(radius / 4.f);

	auto verticalLayout = RenderPipelineLayout{}
		.ComputeShader({ "Utils/GaussianBlur.hlsl", "MainVertical" })
		.Macro({ "KERNEL_RADIUS", radius - 1 })
		.Macro({ "PACKED_WEIGHT_SIZE", packedWeightSize });

	auto horizontalLayout = RenderPipelineLayout{}
		.ComputeShader({ "Utils/GaussianBlur.hlsl", "MainHorizontal" })
		.Macro({ "KERNEL_RADIUS", radius - 1 })
		.Macro({ "PACKED_WEIGHT_SIZE", packedWeightSize });

	// Can't use a traditional bindData structure, since the number of weights are determined at runtime.
	std::vector<uint32_t> bindData;
	bindData.resize(4 + radius);
	bindData[0] = resources.Get(inputTexture);
	bindData[1] = resources.Get(outputTexture);
	// Memcpy to preserve data.
	std::memcpy(bindData.data() + 4, weights.data(), weights.size() * sizeof(float));

	const auto& inputComponent = device->GetResourceManager().Get(resources.GetTexture(inputTexture));

	auto dispatchX = inputComponent.description.width;
	auto dispatchY = std::ceil(inputComponent.description.height / 64.f);

	list.BindPipeline(verticalLayout);
	list.BindConstants("bindData", bindData);
	list.Dispatch(dispatchX, dispatchY, 1);

	// The output texture is about to be read in as the input to the next pass, so synchronize.
	list.UAVBarrier(resources.GetTexture(outputTexture));
	list.FlushBarriers();

	// Use the intermediate results in the output as the input to the second pass.
	bindData[0] = resources.Get(outputTexture);

	dispatchX = std::ceil(inputComponent.description.width / 64.f);
	dispatchY = inputComponent.description.height;

	list.BindPipeline(horizontalLayout);
	list.BindConstants("bindData", bindData);
	list.Dispatch(dispatchX, dispatchY, 1);
}