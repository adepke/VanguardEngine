// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"

struct BindData
{
	uint batchId;
	uint objectBuffer;
	uint cameraBuffer;
	uint cameraIndex;
	uint vertexPositionBuffer;
};

ConstantBuffer<BindData> bindData : register(b0);

struct Input
{
	uint vertexId : SV_VertexID;
	uint instanceId : SV_InstanceID;
};

struct Output
{
	float4 positionCS : SV_POSITION;  // Clip space.
};

[RootSignature(RS)]
Output VSMain(Input input)
{
	StructuredBuffer<ObjectData> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	ObjectData object = objectBuffer[bindData.batchId + input.instanceId];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	VertexAssemblyData assemblyData;
	assemblyData.positionBuffer = bindData.vertexPositionBuffer;
	assemblyData.extraBuffer = 0;  // Unused.
	assemblyData.metadata = object.vertexMetadata;

	Output output;
	output.positionCS = LoadVertexPosition(assemblyData, input.vertexId);
	output.positionCS = mul(output.positionCS, object.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.positionCS = mul(output.positionCS, camera.projection);

	return output;
}