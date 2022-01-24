// Copyright (c) 2019-2022 Andrew Depke

#include "Base.hlsli"

#define RS \
	"RootFlags(0)," \
	"RootConstants(b0, num32BitConstants = 4)," \
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
	uint inputMip;
	uint outputTexture;
	float intensity;
};

ConstantBuffer<BindData> bindData : register(b0);
SamplerState bilinearClampSampler : register(s0);  // Clamp to edge, see: https://www.froyok.fr/blog/2021-12-ue4-custom-bloom/

float3 FilteredSample(Texture2D source, uint level, float2 uv, float2 texelSize)
{	
	// 3x3 Bartlett filter.
	// See: http://advances.realtimerendering.com/s2014/sledgehammer/Next-Generation-Post-Processing-in-Call-of-Duty-Advanced-Warfare-v17.pptx,
	// https://www.researchgate.net/publication/220868865_Pyramid_filters_based_on_bilinear_interpolation

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
	Texture2D<float4> input = textures[bindData.inputTexture];
	RWTexture2D<float4> output = texturesRW[bindData.outputTexture];
	
	float2 outputDimensions;
	output.GetDimensions(outputDimensions.x, outputDimensions.y);
	
	float2 center = (dispatchId.xy + 0.5f) / outputDimensions;
	float2 texelSize = 1.f / outputDimensions;
	
	float3 current = output[dispatchId.xy].rgb;
	float3 upsample = FilteredSample(input, bindData.inputMip, center, texelSize);
	output[dispatchId.xy].rgb = lerp(current, upsample, bindData.intensity);  // See: https://www.froyok.fr/blog/2021-12-ue4-custom-bloom/
}