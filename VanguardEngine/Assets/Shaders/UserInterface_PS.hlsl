// Copyright (c) 2019-2021 Andrew Depke

#include "UserInterface_RS.hlsli"

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

SamplerState sampler0 : register(s0);
Texture2D texture0 : register(t1);

[RootSignature(RS)]
float4 main(PS_INPUT input) : SV_Target
{
	float4 output = input.color * texture0.Sample(sampler0, input.uv);
	return output;
}