// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"
#include "Clusters/Clusters.hlsli"

struct BindData
{
	uint batchId;
	uint objectBuffer;
	uint cameraBuffer;
	uint cameraIndex;
	uint vertexPositionBuffer;
	uint visibilityBuffer;
	float logY;
	float padding;
	int3 dimensions;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VertexIn
{
	uint vertexId : SV_VertexID;
	uint instanceId : SV_InstanceID;
};

struct PSInput
{
	float4 positionCS : SV_POSITION;  // Clip space.
	float depthVS : DEPTH;  // View space.
};

[RootSignature(RS)]
PSInput VSMain(VertexIn input)
{
	StructuredBuffer<ObjectData> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	ObjectData object = objectBuffer[bindData.batchId + input.instanceId];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	VertexAssemblyData assemblyData;
	assemblyData.positionBuffer = bindData.vertexPositionBuffer;
	assemblyData.extraBuffer = 0;  // Unused.
	assemblyData.metadata = object.vertexMetadata;

	PSInput output;
	output.positionCS = LoadVertexPosition(assemblyData, input.vertexId);
	output.positionCS = mul(output.positionCS, object.worldMatrix);
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