#include "Default_RS.hlsli"

struct Input
{
	float4 PositionSS : SV_POSITION;  // Screen space.
	float3 Position : POSITION;  // World space.
	float3 Normal : NORMAL;  // World space.
	float2 UV : UV;
	float3 Tangent : TANGENT;  // World space.
	float3 Bitangent : BITANGENT;  // World space.
	float Depth : DEPTH;  // View space.
};

struct Output
{
	float4 Color;
};

[RootSignature(RS)]
Output main(Input In) : SV_TARGET
{
	Output Out;
	Out.Color = float4(1.f, 1.f, 1.f, 1.f);

	return Out;
}