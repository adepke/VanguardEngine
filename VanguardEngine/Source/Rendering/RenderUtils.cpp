// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/RenderUtils.h>
#include <Rendering/Base.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Asset/TextureLoader.h>
#include <Core/Config.h>

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