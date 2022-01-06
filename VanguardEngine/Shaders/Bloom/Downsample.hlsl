// Copyright (c) 2019-2021 Andrew Depke

#include "Base.hlsli"

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
		"addressW = TEXTURE_ADDRESS_CLAMP)"

struct BindData
{
	uint inputTexture;
	uint outputTexture;
};

ConstantBuffer<BindData> bindData : register(b0);
SamplerState bilinearClampSampler : register(s0);

float3 Average(float3 a, float3 b, float3 c, float3 d)
{
	return (a + b + c + d) * 0.25f;
}

float3 QuadSample(Texture2D source, float2 uv, float2 texelSize)
{
	float3 a = source.SampleLevel(bilinearClampSampler, uv + float2(-texelSize.x, -texelSize.y), 0).rgb;  // Top left.
	float3 b = source.SampleLevel(bilinearClampSampler, uv + float2(texelSize.x, -texelSize.y), 0).rgb;  // Top right.
	float3 c = source.SampleLevel(bilinearClampSampler, uv + float2(-texelSize.x, texelSize.y), 0).rgb;  // Bottom left.
	float3 d = source.SampleLevel(bilinearClampSampler, uv + float2(texelSize.x, texelSize.y), 0).rgb;  // Bottom right.
	
	return Average(a, b, c, d);
}

float3 FilteredSample(Texture2D source, float2 uv, float2 texelSize)
{
	// 36 tap filter, eliminates pulsating artifacts and temporal stability issues.
	// See: http://advances.realtimerendering.com/s2014/sledgehammer/Next-Generation-Post-Processing-in-Call-of-Duty-Advanced-Warfare-v17.pptx
	
	float3 result = 0.5f * QuadSample(source, uv * texelSize, texelSize);  // Center.
	result += 0.125f * QuadSample(source, (uv + float2(-1.f, -1.f)) * texelSize, texelSize);  // Top left.
	result += 0.125f * QuadSample(source, (uv + float2(1.f, -1.f)) * texelSize, texelSize);  // Top right.
	result += 0.125f * QuadSample(source, (uv + float2(-1.f, 1.f)) * texelSize, texelSize);  // Bottom left.
	result += 0.125f * QuadSample(source, (uv + float2(1.f, 1.f)) * texelSize, texelSize);  // Bottom right.
	
	return result;
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
	
	output[dispatchId.xy].rgb = FilteredSample(input, center, texelSize);
}