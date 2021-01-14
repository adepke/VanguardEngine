// Copyright (c) 2019-2021 Andrew Depke

#include "Default_RS.hlsli"

SamplerState defaultSampler : register(s0);
Texture2D albedoMap : register(t1);

struct Input
{
	float4 positionSS : SV_POSITION;  // Screen space.
	float3 position : POSITION;  // World space.
	float3 normal : NORMAL;  // World space.
	float2 uv : UV;
	float3 tangent : TANGENT;  // World space.
	float3 bitangent : BITANGENT;  // World space.
};

struct Output
{
	float4 Color : SV_Target;
};

[RootSignature(RS)]
Output main(Input input)
{
	Output output;
	output.Color = albedoMap.Sample(defaultSampler, input.uv);

	return output;
}