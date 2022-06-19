// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Color.hlsli"

struct BindData
{
	uint mipBase;
	uint mipCount;
	uint inputTextureIndex;
	uint sRGB;
	// Boundary
	uint outputTextureIndex;
	float3 texelSize;
};

ConstantBuffer<BindData> bindData : register(b0);

float4 SRGBAdjustedColor(float4 sample)
{
	if (bindData.sRGB)
	{
		sample.rgb = LinearToSRGB(sample.rgb);
	}
	
	return sample;
}

struct Input
{
	uint groupIndex : SV_GroupIndex;
	uint3 dispatchId : SV_DispatchThreadID;
};

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(Input input)
{
	Texture3D<float4> inputTexture = ResourceDescriptorHeap[bindData.inputTextureIndex];

	float3 uvw = float3(bindData.texelSize * (input.dispatchId.xyz + 0.5.xxx));

	float4 sourceSample = inputTexture.SampleLevel(bilinearWrap, uvw, bindData.mipBase);

	RWTexture3D<float4> outputTexture = ResourceDescriptorHeap[bindData.outputTextureIndex];
	outputTexture[input.dispatchId.xyz] = SRGBAdjustedColor(sourceSample);
}