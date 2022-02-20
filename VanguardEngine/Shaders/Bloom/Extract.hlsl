// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Color.hlsli"

struct BindData
{
	uint inputTexture;
	uint outputTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

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
	Texture2D<float4> input = ResourceDescriptorHeap[bindData.inputTexture];
	RWTexture2D<float3> output = ResourceDescriptorHeap[bindData.outputTexture];
	
	float2 inputDimensions;
	input.GetDimensions(inputDimensions.x, inputDimensions.y);
	
	float2 center = (dispatchId.xy * 2.f + 1.f);
	float2 texelSize = 1.f / inputDimensions;
	
	// 4 bilinear samples to prevent undersampling.
	float3 a = input.Sample(linearMipPointClamp, (center + float2(-1.f, -1.f)) * texelSize).rgb;  // Top left.
	float3 b = input.Sample(linearMipPointClamp, (center + float2(1.f, -1.f)) * texelSize).rgb;  // Top right.
	float3 c = input.Sample(linearMipPointClamp, (center + float2(-1.f, 1.f)) * texelSize).rgb;  // Bottom left.
	float3 d = input.Sample(linearMipPointClamp, (center + float2(1.f, 1.f)) * texelSize).rgb;  // Bottom right.
	
	// Partial Karis average, applied in blocks of 4 samples.
	// See: http://advances.realtimerendering.com/s2014/sledgehammer/Next-Generation-Post-Processing-in-Call-of-Duty-Advanced-Warfare-v17.pptx
	output[dispatchId.xy] = KarisAverage(a, b, c, d);
}