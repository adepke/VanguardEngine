// Copyright (c) 2019-2021 Andrew Depke

#include "UserInterface_RS.hlsli"
#include "Base.hlsli"
#include "Camera.hlsli"

SamplerState defaultSampler : register(s0);

struct BindlessTexture
{
	uint index;
};

ConstantBuffer<BindlessTexture> bindlessTexture : register(b0);

struct DrawData
{
	bool depthLinearization;
};

ConstantBuffer<DrawData> drawData : register(b1);

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

	// If we're rendering a depth texture, we may want to linearize it.
	if (drawData.depthLinearization)
	{
		output.xyz = 1.f - LinearizeDepth(output.x);
		output.w = 1.f;
	}

	return output;
}