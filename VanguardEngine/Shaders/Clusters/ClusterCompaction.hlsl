// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"

struct BindData
{
	int3 gridDimensions;
	uint visibilityBuffer;
	uint denseListBuffer;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(64, 1, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	// #TODO: Consider using LDS (groupshared) for atomic counting, then update the result buffer at the end.

	Buffer<uint> clusterVisibilityBuffer = ResourceDescriptorHeap[bindData.visibilityBuffer];
	RWStructuredBuffer<uint> denseClusterList = ResourceDescriptorHeap[bindData.denseListBuffer];
	
	if (dispatchId.x < bindData.gridDimensions.x * bindData.gridDimensions.y * bindData.gridDimensions.z)
	{
		if (clusterVisibilityBuffer[dispatchId.x])
		{
			// Increment the count and store the cluster id.
			uint i = denseClusterList.IncrementCounter();
			denseClusterList[i] = dispatchId.x;
		}
	}
}