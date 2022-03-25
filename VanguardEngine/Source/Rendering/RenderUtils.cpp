// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/RenderUtils.h>
#include <Rendering/Base.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>

#include <vector>
#include <cstring>
#include <cmath>

void RenderUtils::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	ComputePipelineStateDescription clearUAVStateDesc{};
	clearUAVStateDesc.shader = { "ClearUAV.hlsl", "ClearUAVMain" };
	clearUAVState.Build(*device, clearUAVStateDesc);
}

void RenderUtils::ClearUAV(CommandList& list, BufferHandle buffer, const DescriptorHandle& descriptor)
{
	VGScopedGPUStat("Clear UAV", device->GetDirectContext(), list.Native());

	auto& bufferComponent = device->GetResourceManager().Get(buffer);

	// Only non-structured buffers can benefit from hardware fast path for clears.
	if (bufferComponent.description.format)
	{
		constexpr uint32_t clearValues[4] = { 0 };
		// Pass the shader-visible descriptor in as the GPU handle, but the non-visible descriptor for the CPU handle.
		list.Native()->ClearUnorderedAccessViewUint(*bufferComponent.UAV, descriptor, bufferComponent.Native(), clearValues, 0, nullptr);
	}

	else
	{
		list.BindPipelineState(clearUAVState);
		list.BindResource("bufferUint", buffer);

		struct ClearUAVInfo
		{
			uint32_t bufferSize;
			XMFLOAT3 padding;
		} clearUAVInfo;
		clearUAVInfo.bufferSize = bufferComponent.description.size;

		list.BindConstants("clearUAVInfo", clearUAVInfo);

		uint32_t dispatchSize = static_cast<uint32_t>(std::ceil(bufferComponent.description.size / 64.f));
		list.Dispatch(dispatchSize, 1, 1);
	}
}