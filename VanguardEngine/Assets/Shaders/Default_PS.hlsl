#include "Default_RS.hlsi"

struct Input
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

struct Output
{
	float4 Color;
};

[RootSignature(RS)]
Output main(Input In) : SV_TARGET
{
	Output Out;
	Out.Color = In.Color;

	return Out;
}