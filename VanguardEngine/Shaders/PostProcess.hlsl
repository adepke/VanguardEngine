// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "ToneMapping.hlsli"

struct BindData
{
	uint mainOutput;
	float3 padding;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VSInput
{
	uint vertexID : SV_VertexID;
};

struct PSInput
{
	float4 positionCS : SV_POSITION;
	float2 uv : UV;
};

[RootSignature(RS)]
PSInput VSMain(VSInput input)
{
	PSInput output;
	output.uv = float2((input.vertexID << 1) & 2, input.vertexID & 2);
	output.positionCS = float4((output.uv.x - 0.5) * 2.0, -(output.uv.y - 0.5) * 2.0, 0, 1);

	return output;
}

[RootSignature(RS)]
float4 PSMain(PSInput input) : SV_Target
{
	Texture2D<float4> mainOutput = ResourceDescriptorHeap[bindData.mainOutput];
	const float4 mainOutputHDR = mainOutput.Sample(bilinearWrap, input.uv);
	const float3 toneMapped = ToneMap(mainOutputHDR.rgb);

	return float4(toneMapped, 1.f);
}