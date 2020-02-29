#include "Default_RS.hlsli"

struct Input
{
	float3 Position : POSITION;
	float4 Color : COLOR;
};

struct Output
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

[RootSignature(RS)]
Output main(Input In)
{
	Output Out;
	Out.Position = float4(In.Position.x, In.Position.y, In.Position.z, 1.f);
	Out.Color = In.Color;

	return Out;
}