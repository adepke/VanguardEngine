// Copyright (c) 2019-2021 Andrew Depke

#include "Default_RS.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"

ConstantBuffer<PerObject> perObject : register(b1);
ConstantBuffer<Camera> camera : register(b2);

struct Input
{
	uint vertexID : SV_VertexID;
};

struct Output
{
	float4 positionCS : SV_POSITION;  // Clip space.
	float3 position : POSITION;  // World space.
	float3 normal : NORMAL;  // World space.
	float2 uv : UV;
	float3 tangent : TANGENT;  // World space.
	float3 bitangent : BITANGENT;  // World space.
	float depthVS : DEPTH;  // View space.
};

[RootSignature(RS)]
Output main(Input input)
{
    float4 position = LoadVertexPosition(input.vertexID);
	float4 normal = float4(LoadVertexNormal(input.vertexID), 0.f);
	float2 uv = LoadVertexTexcoord(input.vertexID);
	float4 tangent = LoadVertexTangent(input.vertexID);
	float4 bitangent = LoadVertexBitangent(input.vertexID);
	
	Output output;
	output.positionCS = position;
	output.positionCS = mul(output.positionCS, perObject.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.depthVS = output.positionCS.z;
	output.positionCS = mul(output.positionCS, camera.projection);
	output.position = mul(position, perObject.worldMatrix).xyz;
	output.normal = normalize(mul(normal, perObject.worldMatrix)).xyz;
	output.uv = uv;
	output.tangent = normalize(mul(tangent, perObject.worldMatrix)).xyz;
	output.bitangent = normalize(mul(bitangent, perObject.worldMatrix)).xyz;

	return output;
}