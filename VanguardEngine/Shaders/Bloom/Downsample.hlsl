// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"

struct BindData
{
	uint inputTexture;
	uint outputTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

float3 Average(float3 a, float3 b, float3 c, float3 d)
{
	return (a + b + c + d) * 0.25f;
}

template <typename T>
float3 QuadSample(T source, float2 uv, float2 texelSize)
{
	float3 a = source.SampleLevel(downsampleBorder, uv + float2(-texelSize.x, -texelSize.y), 0).rgb;  // Top left.
	float3 b = source.SampleLevel(downsampleBorder, uv + float2(texelSize.x, -texelSize.y), 0).rgb;  // Top right.
	float3 c = source.SampleLevel(downsampleBorder, uv + float2(-texelSize.x, texelSize.y), 0).rgb;  // Bottom left.
	float3 d = source.SampleLevel(downsampleBorder, uv + float2(texelSize.x, texelSize.y), 0).rgb;  // Bottom right.
	
	return Average(a, b, c, d);
}

template <typename T>
float3 FilteredSample(T source, float2 location, float2 texelSize)
{
	// 36 tap filter, eliminates pulsating artifacts and temporal stability issues.
	// See: http://advances.realtimerendering.com/s2014/sledgehammer/Next-Generation-Post-Processing-in-Call-of-Duty-Advanced-Warfare-v17.pptx
	
	float3 result = 0.5f * QuadSample(source, location * texelSize, texelSize);  // Center.
	result += 0.125f * QuadSample(source, (location + float2(-1.f, -1.f)) * texelSize, texelSize);  // Top left.
	result += 0.125f * QuadSample(source, (location + float2(1.f, -1.f)) * texelSize, texelSize);  // Top right.
	result += 0.125f * QuadSample(source, (location + float2(-1.f, 1.f)) * texelSize, texelSize);  // Bottom left.
	result += 0.125f * QuadSample(source, (location + float2(1.f, 1.f)) * texelSize, texelSize);  // Bottom right.
	
	return result;
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	Texture2D<float3> input = ResourceDescriptorHeap[bindData.inputTexture];
	RWTexture2D<float3> output = ResourceDescriptorHeap[bindData.outputTexture];
	
	float2 inputDimensions;
	input.GetDimensions(inputDimensions.x, inputDimensions.y);
	
	float2 center = (dispatchId.xy * 2.f + 1.f);
	float2 texelSize = 1.f / inputDimensions;
	
	output[dispatchId.xy] = FilteredSample(input, center, texelSize);
}