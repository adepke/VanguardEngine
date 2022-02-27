// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"

struct BindData
{
	uint denseClusterListBuffer;
	uint indirectBuffer;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(1, 1, 1)]
void Main()
{
	RWStructuredBuffer<uint> denseClusterListBuffer = ResourceDescriptorHeap[bindData.denseClusterListBuffer];
	RWStructuredBuffer<uint3> indirectBuffer = ResourceDescriptorHeap[bindData.indirectBuffer];

	uint activeClusters = denseClusterListBuffer.IncrementCounter();
	indirectBuffer[0] = uint3(activeClusters, 1, 1);  // Dispatch one group for each active cluster.
}