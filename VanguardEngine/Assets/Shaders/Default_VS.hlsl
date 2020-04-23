#include "Default_RS.hlsli"

// #TODO: Replace with ConstantBuffer in DXC branch.
cbuffer CameraBuffer : register(b0)
{
	matrix ViewMatrix;
	matrix ProjectionMatrix;
};

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
	Out.Position = mul(Out.Position, ViewMatrix);
	Out.Position = mul(Out.Position, ProjectionMatrix);
	Out.Color = In.Color;

	return Out;
}