// Copyright (c) 2019-2021 Andrew Depke

#include "Prepass_RS.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"

ConstantBuffer<PerObject> perObject : register(b0);
ConstantBuffer<Camera> camera : register(b1);

struct Vertex
{
	float3 position : POSITION;  // Object space.
	float3 normal : NORMAL;  // Object space.
	float2 uv : UV;
	float3 tangent : TANGENT;  // Object space.
	float3 bitangent : BITANGENT;  // Object space.
};

StructuredBuffer<Vertex> vertexBuffer : register(t0);

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
	Vertex vertex = vertexBuffer[input.vertexID];

	Output output;
	output.positionCS = float4(vertex.position, 1.f);
	output.positionCS = mul(output.positionCS, perObject.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.positionCS = mul(output.positionCS, camera.projection);

	return output;
}