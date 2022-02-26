// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"

struct BindData
{
	uint objectBuffer;
	uint objectIndex;
	uint cameraBuffer;
	uint cameraIndex;
	VertexAssemblyData vertexAssemblyData;
};

ConstantBuffer<BindData> bindData : register(b0);

struct Input
{
	uint vertexID : SV_VertexID;
};

struct Output
{
	float4 positionCS : SV_POSITION;  // Clip space.
};

[RootSignature(RS)]
Output VSMain(Input input)
{
	StructuredBuffer<PerObject> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	PerObject perObject = objectBuffer[bindData.objectIndex];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	Output output;
	output.positionCS = LoadVertexPosition(bindData.vertexAssemblyData, input.vertexID);
	output.positionCS = mul(output.positionCS, perObject.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.positionCS = mul(output.positionCS, camera.projection);

	return output;
}