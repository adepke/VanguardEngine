// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"

struct BindData
{
	uint bufferHandle;
	uint bufferSize;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(64, 1, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	RWStructuredBuffer<uint> bufferUint = ResourceDescriptorHeap[bindData.bufferHandle];

	if (dispatchId.x < bindData.bufferSize)
	{
		bufferUint[dispatchId.x] = 0;
	}
}