// Copyright (c) 2019-2021 Andrew Depke

#include "Base.hlsli"
#include "Color.hlsli"

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
	uint inputTexture;
	uint outputTexture;
	float threshold;
};

ConstantBuffer<BindData> bindData : register(b0);
SamplerState bilinearClampSampler : register(s0);

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	Texture2D<float4> input = textures[bindData.inputTexture];
	RWTexture2D<float4> output = texturesRW[bindData.outputTexture];
	
	float2 outputDimensions;
	output.GetDimensions(outputDimensions.x, outputDimensions.y);
	
	float2 center = (dispatchId.xy + 0.5f);
	float2 texelSize = 1.f / outputDimensions;
	
	// 4 bilinear samples to prevent undersampling.
	float3 a = input.Sample(bilinearClampSampler, (center + float2(-0.25f, -0.25f)) * texelSize).rgb;  // Top left.
	float3 b = input.Sample(bilinearClampSampler, (center + float2(0.25f, -0.25f)) * texelSize).rgb;  // Top right.
	float3 c = input.Sample(bilinearClampSampler, (center + float2(-0.25f, 0.25f)) * texelSize).rgb;  // Bottom left.
	float3 d = input.Sample(bilinearClampSampler, (center + float2(0.25f, 0.25f)) * texelSize).rgb;  // Bottom right.
	
	const float luminanceA = LinearToLuminance(a);
	const float luminanceB = LinearToLuminance(b);
	const float luminanceC = LinearToLuminance(c);
	const float luminanceD = LinearToLuminance(d);
	
	// Brightness filter pass, reduces contribution from fireflies.
	// See: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/BloomExtractAndDownsampleHdrCS.hlsl
	const float epsilon = 0.0001f;
	a *= max(epsilon, luminanceA - bindData.threshold) / (luminanceA + epsilon);
	b *= max(epsilon, luminanceB - bindData.threshold) / (luminanceB + epsilon);
	c *= max(epsilon, luminanceC - bindData.threshold) / (luminanceC + epsilon);
	d *= max(epsilon, luminanceD - bindData.threshold) / (luminanceD + epsilon);
	
	output[dispatchId.xy] = float4((a + b + c + d) * 0.25f, 0.f);
}