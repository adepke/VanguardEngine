// Copyright (c) 2019-2021 Andrew Depke

#include "UserInterface_RS.hlsli"
#include "Base.hlsli"

SamplerState defaultSampler : register(s0);

struct BindlessTexture
{
	uint index;
};

ConstantBuffer<BindlessTexture> bindlessTexture : register(b0);

struct Input
{
	float4 position : SV_POSITION;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

[RootSignature(RS)]
float4 main(Input input) : SV_Target
{
	Texture2D textureTarget = textures[bindlessTexture.index];
	float4 output = input.color * textureTarget.Sample(defaultSampler, input.uv);
	return output;
}