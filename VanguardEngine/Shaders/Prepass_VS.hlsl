// Copyright (c) 2019-2021 Andrew Depke

#include "Prepass_RS.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"

ConstantBuffer<PerObject> perObject : register(b0);
ConstantBuffer<Camera> camera : register(b1);

struct Input
{
	uint vertexID : SV_VertexID;
};

struct Output
{
	float4 positionCS : SV_POSITION;  // Clip space.
};

[RootSignature(RS)]
Output main(Input input)
{
	Output output;
    output.positionCS = LoadVertexPosition(input.vertexID);
	output.positionCS = mul(output.positionCS, perObject.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.positionCS = mul(output.positionCS, camera.projection);

	return output;
}