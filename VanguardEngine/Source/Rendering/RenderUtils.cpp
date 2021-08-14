// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/RenderUtils.h>
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

void RenderUtils::ClearUAV(CommandList& list, BufferHandle buffer)
{
	list.BindPipelineState(clearUAVState);
	list.BindResource("bufferUint", buffer);

	struct ClearUAVInfo
	{
		uint32_t bufferSize;
		XMFLOAT3 padding;
	} clearUAVInfo;

	auto& bufferComponent = device->GetResourceManager().Get(buffer);
	clearUAVInfo.bufferSize = bufferComponent.description.size;
	std::vector<uint32_t> clearUAVInfoData;
	clearUAVInfoData.resize(sizeof(ClearUAVInfo) / 4);
	std::memcpy(clearUAVInfoData.data(), &clearUAVInfo, clearUAVInfoData.size() * sizeof(uint32_t));

	list.BindConstants("clearUAVInfo", clearUAVInfoData);

	uint32_t dispatchSize = static_cast<uint32_t>(std::ceil(bufferComponent.description.size / 64.f));
	list.Native()->Dispatch(dispatchSize, 1, 1);
}