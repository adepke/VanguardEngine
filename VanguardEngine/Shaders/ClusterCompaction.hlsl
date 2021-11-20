// Copyright (c) 2019-2021 Andrew Depke

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"RootConstants(b0, num32BitConstants = 4)," \
	"SRV(t0)," \
	"DescriptorTable(" \
		"UAV(u0))"

struct ClusterData
{
	int3 gridDimensions;
	float padding;
};

ConstantBuffer<ClusterData> clusterData : register(b0);
StructuredBuffer<bool> clusterVisibility : register(t0);
RWStructuredBuffer<uint> denseClusterList : register(u0);

[RootSignature(RS)]
[numthreads(64, 1, 1)]
void ComputeDenseClusterListMain(uint3 dispatchId : SV_DispatchThreadID)
{
	// #TODO: Consider using LDS (groupshared) for atomic counting, then update the result buffer at the end.
	
	if (dispatchId.x < clusterData.gridDimensions.x * clusterData.gridDimensions.y * clusterData.gridDimensions.z)
	{
		if (clusterVisibility[dispatchId.x])
		{
			// Increment the count and store the cluster id.
			uint i = denseClusterList.IncrementCounter();
			denseClusterList[i] = dispatchId.x;
		}
	}
}