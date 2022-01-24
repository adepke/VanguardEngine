// Copyright (c) 2019-2022 Andrew Depke

#include "Camera.hlsli"
#include "Clusters/Clusters.hlsli"

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"RootConstants(b0, num32BitConstants = 4)," \
	"SRV(t0, visibility = SHADER_VISIBILITY_PIXEL)," \
	"SRV(t1, visibility = SHADER_VISIBILITY_PIXEL)"

struct ClusterData
{
	int3 gridDimensions;
	float logY;
};

ConstantBuffer<ClusterData> clusterData : register(b0);
StructuredBuffer<uint2> clusterLightInfo : register(t0);
StructuredBuffer<bool> clusterVisibility : register(t1);

struct VSOutput
{
	float4 positionCS : SV_POSITION;  // Clip space.
};

[RootSignature(RS)]
VSOutput VSMain(uint vertexID : SV_VertexID)
{
	VSOutput output;
	float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
	output.positionCS = float4((uv.x - 0.5) * 2.0, -(uv.y - 0.5) * 2.0, 0, 1);  // Z of 0 due to the inverse depth.
	
	return output;
}

[RootSignature(RS)]
float4 PSMain(VSOutput input) : SV_Target
{
	uint maxLightCount = 0;
	
	// Walk through all active Z bins in this tile and determine the most filled bin.
	for (int i = 0; i < clusterData.gridDimensions.z; ++i)
	{
		uint clusterIndex = ClusterId2Index(clusterData.gridDimensions, uint3(input.positionCS.x / FROXEL_SIZE, input.positionCS.y / FROXEL_SIZE, i));
		
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