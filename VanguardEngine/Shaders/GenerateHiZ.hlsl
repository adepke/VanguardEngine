// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"

struct BindData
{
	uint mipBase;
	uint mipCount;
	float2 texelSize;
	// Boundary
	uint4 outputTextureIndices;
	// Boundary
	uint inputTextureIndex;
};

ConstantBuffer<BindData> bindData : register(b0);

groupshared float groupColor[64];

void StoreGroupColor(uint index, float color)
{
	groupColor[index] = color;
}

float LoadGroupColor(uint index)
{
	return groupColor[index];
}

struct Input
{
	uint groupIndex : SV_GroupIndex;
	uint3 dispatchId : SV_DispatchThreadID;
};

// Similar to generate mip maps, but applies minification instead of mean sampling.

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(Input input)
{ 
	Texture2D<float4> inputTexture = ResourceDescriptorHeap[bindData.inputTextureIndex];
	
	float2 uv = bindData.texelSize * (input.dispatchId.xy + float2(0.5, 0.5));

	// Single sample, hi-z texture dimensions are a power of 2.
	float4 sourceSample = inputTexture.SampleLevel(linearMipPointClampMinimum, uv, bindData.mipBase);
	
	RWTexture2D<float> outputMip0 = ResourceDescriptorHeap[bindData.outputTextureIndices[0]];
	outputMip0[input.dispatchId.xy] = sourceSample;
	
	if (bindData.mipCount == 1)
	{
		return;
	}
	
	StoreGroupColor(input.groupIndex, sourceSample);
	
	GroupMemoryBarrierWithGroupSync();
	
	// X and Y are even.
	if ((input.groupIndex & 0x9) == 0)
	{
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x01));
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x08));
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x09));
		
		RWTexture2D<float> outputMip1 = ResourceDescriptorHeap[bindData.outputTextureIndices[1]];
		outputMip1[input.dispatchId.xy / 2] = sourceSample;
		
		StoreGroupColor(input.groupIndex, sourceSample);
	}
	
	if (bindData.mipCount == 2)
	{
		return;
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// X and Y are multiples of 4.
	if ((input.groupIndex & 0x1B) == 0)
	{
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x02));
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x10));
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x12));
		
		RWTexture2D<float> outputMip2 = ResourceDescriptorHeap[bindData.outputTextureIndices[2]];
		outputMip2[input.dispatchId.xy / 4] = sourceSample;
		
		StoreGroupColor(input.groupIndex, sourceSample);
	}
	
	if (bindData.mipCount == 3)
	{
		return;
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// X and Y are multiples of 8 (only one thread qualifies).
	if (input.groupIndex == 0)
	{
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x04));
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x20));
		sourceSample = min(sourceSample, LoadGroupColor(input.groupIndex + 0x24));
		
		RWTexture2D<float> outputMip3 = ResourceDescriptorHeap[bindData.outputTextureIndices[3]];
		outputMip3[input.dispatchId.xy / 8] = sourceSample;
	}
}