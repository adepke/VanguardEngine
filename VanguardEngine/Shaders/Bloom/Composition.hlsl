// Copyright (c) 2019-2021 Andrew Depke

#include "Base.hlsli"

#define RS \
	"RootFlags(0)," \
	"RootConstants(b0, num32BitConstants = 3)," \
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
	uint hdrSource;
	uint bloomSamples;
	uint bloomMips;
};

ConstantBuffer<BindData> bindData : register(b0);
SamplerState bilinearClampSampler : register(s0);

float3 FilteredSample(Texture2D source, uint level, float2 uv, float2 texelSize)
{
	// 3x3 tent filter. See: http://advances.realtimerendering.com/s2014/sledgehammer/Next-Generation-Post-Processing-in-Call-of-Duty-Advanced-Warfare-v17.pptx

	float3 result = 1.f * source.SampleLevel(bilinearClampSampler, uv + float2(-texelSize.x, -texelSize.y), level).rgb;
	result += 2.f * source.SampleLevel(bilinearClampSampler, uv + float2(0.f, -texelSize.y), level).rgb;
	result += 1.f * source.SampleLevel(bilinearClampSampler, uv + float2(texelSize.x, -texelSize.y), level).rgb;
	result += 2.f * source.SampleLevel(bilinearClampSampler, uv + float2(-texelSize.x, 0.f), level).rgb;
	result += 4.f * source.SampleLevel(bilinearClampSampler, uv + float2(0.f, 0.f), level).rgb;
	result += 2.f * source.SampleLevel(bilinearClampSampler, uv + float2(texelSize.x, 0.f), level).rgb;
	result += 1.f * source.SampleLevel(bilinearClampSampler, uv + float2(-texelSize.x, texelSize.y), level).rgb;
	result += 2.f * source.SampleLevel(bilinearClampSampler, uv + float2(0.f, texelSize.y), level).rgb;
	result += 1.f * source.SampleLevel(bilinearClampSampler, uv + float2(texelSize.x, texelSize.y), level).rgb;

	return result / 16.f;
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture2D<float4> hdrSource = texturesRW[bindData.hdrSource];
	Texture2D<float4> bloomSamples = textures[bindData.bloomSamples];
	
	uint width, height;
	bloomSamples.GetDimensions(width, height);
	
	float2 uv = dispatchId.xy / (float2(width, height) * 2.f);
	float3 pixel = 0.f;
	
	// Progressive filtered upsample.
	
	for (int i = bindData.bloomMips; i >= 0; --i)
	{
		float mipWidth, mipHeight, mipLevels;
		bloomSamples.GetDimensions(i, mipWidth, mipHeight, mipLevels);
		
		float2 texelSize = 1.f / float2(mipWidth, mipHeight);
		
		float3 mipSample = FilteredSample(bloomSamples, i, uv, texelSize);
		pixel += mipSample;
	}
	
	hdrSource[dispatchId.xy].rgb += pixel;
}