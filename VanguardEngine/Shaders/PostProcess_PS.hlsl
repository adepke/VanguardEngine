// Copyright (c) 2019-2021 Andrew Depke

#include "PostProcess_RS.hlsli"
#include "Base.hlsli"
#include "ToneMapping.hlsli"

SamplerState defaultSampler : register(s0);

struct InputTextures
{
	uint mainOutput;
	float3 padding;
};

ConstantBuffer<InputTextures> inputTextures : register(b0);

struct Input
{
	float4 positionCS : SV_POSITION;
	float2 uv : UV;
};

struct Output
{
	float4 color : SV_Target;
};

[RootSignature(RS)]
Output main(Input input)
{
	const float4 mainOutputHDR = textures[inputTextures.mainOutput].Sample(defaultSampler, input.uv);

	const float3 toneMapped = ToneMap(mainOutputHDR.rgb);

	Output output;
	output.color.rgb = toneMapped;
	output.color.a = 1.0;

	return output;
}