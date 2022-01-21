// Copyright (c) 2019-2021 Andrew Depke

#include "Base.hlsli"
#include "Color.hlsli"

#define RS \
	"RootFlags(0)," \
	"RootConstants(b0, num32BitConstants = 2)," \
	"DescriptorTable(" \
		"SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"DescriptorTable(" \
		"UAV(u0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"StaticSampler(" \
		"s0," \
		"filter = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP," \
		"comparisonFunc = COMPARISON_ALWAYS)"

struct BindData
{
	uint inputTexture;
	uint outputTexture;
};

ConstantBuffer<BindData> bindData : register(b0);
SamplerState bilinearClampSampler : register(s0);

float3 KarisAverage(float3 a, float3 b, float3 c, float3 d)
{
	// See: https://graphicrants.blogspot.com/2013/12/tone-mapping.html
	
	float3 weightA = 1.f / (1.f + LinearToLuminance(a));
	float3 weightB = 1.f / (1.f + LinearToLuminance(b));
	float3 weightC = 1.f / (1.f + LinearToLuminance(c));
	float3 weightD = 1.f / (1.f + LinearToLuminance(d));
	
	return (a * weightA + b * weightB + c * weightC + d * weightD) / (weightA + weightB + weightC + weightD);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	Texture2D<float4> input = textures[bindData.inputTexture];
	RWTexture2D<float4> output = texturesRW[bindData.outputTexture];
	
	float2 inputDimensions;
	input.GetDimensions(inputDimensions.x, inputDimensions.y);
	
	float2 center = (dispatchId.xy * 2.f + 1.f);
	float2 texelSize = 1.f / inputDimensions;
	
	// 4 bilinear samples to prevent undersampling.
	float3 a = input.Sample(bilinearClampSampler, (center + float2(-1.f, -1.f)) * texelSize).rgb;  // Top left.
	float3 b = input.Sample(bilinearClampSampler, (center + float2(1.f, -1.f)) * texelSize).rgb;  // Top right.
	float3 c = input.Sample(bilinearClampSampler, (center + float2(-1.f, 1.f)) * texelSize).rgb;  // Bottom left.
	float3 d = input.Sample(bilinearClampSampler, (center + float2(1.f, 1.f)) * texelSize).rgb;  // Bottom right.
	
	output[dispatchId.xy].rgb = KarisAverage(a, b, c, d);
}