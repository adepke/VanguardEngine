// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"
#include "Clusters/Clusters.hlsli"

struct BindData
{
	uint objectBuffer;
	uint objectIndex;
	uint cameraBuffer;
	uint cameraIndex;
	VertexAssemblyData vertexAssemblyData;
	uint visibilityBuffer;
	int3 dimensions;
	float logY;
};

ConstantBuffer<BindData> bindData : register(b0);

struct PSInput
{
	float4 positionCS : SV_POSITION;  // Clip space.
	float depthVS : DEPTH;  // View space.
};

[RootSignature(RS)]
PSInput VSMain(uint vertexId : SV_VertexID)
{
	StructuredBuffer<PerObject> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	PerObject perObject = objectBuffer[bindData.objectIndex];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	PSInput output;
	output.positionCS = LoadVertexPosition(bindData.vertexAssemblyData, vertexId);
	output.positionCS = mul(output.positionCS, perObject.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.depthVS = output.positionCS.z;
	output.positionCS = mul(output.positionCS, camera.projection);
	
	return output;
}

[RootSignature(RS)]
[earlydepthstencil]
void PSMain(PSInput input)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	RWBuffer<uint> clusterVisibilityBuffer = ResourceDescriptorHeap[bindData.visibilityBuffer];

	// Fully depth culled clusters will have their visibility flag set to false.
	uint3 clusterId = DrawToClusterId(FROXEL_SIZE, bindData.logY, camera, input.positionCS.xy, input.depthVS);
	clusterVisibilityBuffer[ClusterId2Index(bindData.dimensions, clusterId)] = true;
}