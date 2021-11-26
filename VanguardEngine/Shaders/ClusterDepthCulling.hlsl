// Copyright (c) 2019-2021 Andrew Depke

#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"
#include "Clusters.hlsli"

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"RootConstants(b0, num32BitConstants = 4)," \
	"CBV(b1, visibility = SHADER_VISIBILITY_VERTEX)," \
	"CBV(b2, visibility = SHADER_VISIBILITY_ALL)," \
	"UAV(u0, visibility = SHADER_VISIBILITY_PIXEL)," \
	"SRV(t0, space = 3, visibility = SHADER_VISIBILITY_VERTEX)," \
	"SRV(t1, space = 3, visibility = SHADER_VISIBILITY_VERTEX)," \
	"CBV(b0, space = 3, visibility = SHADER_VISIBILITY_VERTEX)"

struct ClusterData
{
	int3 gridDimensions;
	float logY;
};

ConstantBuffer<ClusterData> clusterData : register(b0);
ConstantBuffer<PerObject> perObject : register(b1);
ConstantBuffer<Camera> camera : register(b2);

RWStructuredBuffer<bool> clusterVisibility : register(u0);  // Root descriptor UAVs must be structured buffers. #TODO: Use RWBuffer with bindless.

struct VSOutput
{
	float4 positionCS : SV_POSITION;  // Clip space.
	float depthVS : DEPTH;  // View space.
};

[RootSignature(RS)]
VSOutput VSMain(uint vertexId : SV_VertexID)
{
	VSOutput output;
	output.positionCS = LoadVertexPosition(vertexId);
	output.positionCS = mul(output.positionCS, perObject.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.depthVS = output.positionCS.z;
	output.positionCS = mul(output.positionCS, camera.projection);
	
	return output;
}

[RootSignature(RS)]
[earlydepthstencil]
void PSMain(VSOutput input)
{
	// Fully depth culled clusters will have their visibility flag set to false.
	uint3 clusterId = DrawToClusterId(FROXEL_SIZE, clusterData.logY, camera, input.positionCS.xy, input.depthVS);
	clusterVisibility[ClusterId2Index(clusterData.gridDimensions, clusterId)] = true;
}