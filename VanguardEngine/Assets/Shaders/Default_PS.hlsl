#include "Default_RS.hlsli"

SamplerState Sampler : register(s0);
Texture2D AlbedoMap : register(t1);
Texture2D NormalMap : register(t2);
Texture2D RoughnessMap : register(t3);
Texture2D MetallicMap : register(t4);

struct Input
{
	float4 PositionSS : SV_POSITION;  // Screen space.
	float3 Position : POSITION;  // World space.
	float3 Normal : NORMAL;  // World space.
	float2 UV : UV;
	float3 Tangent : TANGENT;  // World space.
	float3 Bitangent : BITANGENT;  // World space.
};

struct Output
{
	float4 Color;
};

[RootSignature(RS)]
Output main(Input In) : SV_TARGET
{
	Output Out;
	Out.Color = AlbedoMap.Sample(Sampler, In.UV);

	return Out;
}