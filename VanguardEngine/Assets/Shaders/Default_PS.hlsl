#include "Default_RS.hlsli"

SamplerState defaultSampler : register(s0);
Texture2D albedoMap : register(t1);
Texture2D normalMap : register(t2);
Texture2D roughnessMap : register(t3);
Texture2D metallicMap : register(t4);

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
	float4 Color;
};

[RootSignature(RS)]
Output main(Input input) : SV_TARGET
{
	Output output;
	output.Color = albedoMap.Sample(defaultSampler, input.uv);

	return output;
}