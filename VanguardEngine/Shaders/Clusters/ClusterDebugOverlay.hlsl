// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Clusters/Clusters.hlsli"

struct BindData
{
	int3 gridDimensions;
	float logY;
	uint lightInfoBuffer;
	uint visibilityBuffer;
};

ConstantBuffer<BindData> bindData : register(b0);

struct PixelIn
{
	float4 positionCS : SV_POSITION;  // Clip space.
};

[RootSignature(RS)]
PixelIn VSMain(uint vertexID : SV_VertexID)
{
	PixelIn output;
	float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
	output.positionCS = float4((uv.x - 0.5) * 2.0, -(uv.y - 0.5) * 2.0, 0, 1);  // Z of 0 due to the inverse depth.
	
	return output;
}

[RootSignature(RS)]
float4 PSMain(PixelIn input) : SV_Target
{
	StructuredBuffer<uint2> clusterLightInfo = ResourceDescriptorHeap[bindData.lightInfoBuffer];
	Buffer<uint> clusterVisibility = ResourceDescriptorHeap[bindData.visibilityBuffer];

	uint maxLightCount = 0;
	
	// Walk through all active Z bins in this tile and determine the most filled bin.
	for (int i = 0; i < bindData.gridDimensions.z; ++i)
	{
		uint clusterIndex = ClusterId2Index(bindData.gridDimensions, uint3(input.positionCS.x / FROXEL_SIZE, input.positionCS.y / FROXEL_SIZE, i));
		
		// Inactive bins are not cleared, so they will falsely contribute.
		if (clusterVisibility[clusterIndex])
		{
			uint lightCount = clusterLightInfo[clusterIndex].y;
			maxLightCount = max(maxLightCount, lightCount);
		}
	}
	
	float binFullness = (float)maxLightCount / (float)MAX_LIGHTS_PER_FROXEL;
	
	// Green = empty, red = full.
	return float4(binFullness, 1.f - binFullness, 0.f, 1.f);
}