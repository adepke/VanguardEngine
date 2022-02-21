// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Color.hlsli"

struct BindData
{
	uint mipBase;
	uint mipCount;
	float2 texelSize;
	// Boundary
	uint4 outputTextureIndices;
	// Boundary
	uint inputTextureIndex;
	uint sRGB;
	uint array;
	uint layer;
};

ConstantBuffer<BindData> bindData : register(b0);

// Separated channels to reduce bank conflicts.
groupshared float groupRed[64];
groupshared float groupGreen[64];
groupshared float groupBlue[64];
groupshared float groupAlpha[64];

void StoreGroupColor(uint index, float4 color)
{
	groupRed[index] = color.r;
	groupGreen[index] = color.g;
	groupBlue[index] = color.b;
	groupAlpha[index] = color.a;
}

float4 LoadGroupColor(uint index)
{
	return float4(groupRed[index], groupGreen[index], groupBlue[index], groupAlpha[index]);
}

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
void main(Input input)
{ 
	float2 offset = bindData.texelSize * 0.5;
	
	float4 sourceSample;
	
	if (!bindData.array)
	{
		Texture2D<float4> inputTexture = ResourceDescriptorHeap[bindData.inputTextureIndex];
		
		float2 uv = bindData.texelSize * (input.dispatchId.xy + float2(0.25, 0.25));
	
		// Perform 4 samples to prevent undersampling non power of two textures.
		sourceSample = inputTexture.SampleLevel(bilinearClamp, uv, bindData.mipBase);
		sourceSample += inputTexture.SampleLevel(bilinearClamp, uv + float2(offset.x, 0.0), bindData.mipBase);
		sourceSample += inputTexture.SampleLevel(bilinearClamp, uv + float2(0.0, offset.y), bindData.mipBase);
		sourceSample += inputTexture.SampleLevel(bilinearClamp, uv + float2(offset.x, offset.y), bindData.mipBase);
	}
	
	else
	{
		Texture2DArray<float4> inputTexture = ResourceDescriptorHeap[bindData.inputTextureIndex];
		
		float3 uv = float3(bindData.texelSize * (input.dispatchId.xy + float2(0.25, 0.25)), bindData.layer);
	
		// Perform 4 samples to prevent undersampling non power of two textures.
		sourceSample = inputTexture.SampleLevel(bilinearClamp, uv, bindData.mipBase);
		sourceSample += inputTexture.SampleLevel(bilinearClamp, uv + float3(offset.x, 0.0, 0.0), bindData.mipBase);
		sourceSample += inputTexture.SampleLevel(bilinearClamp, uv + float3(0.0, offset.y, 0.0), bindData.mipBase);
		sourceSample += inputTexture.SampleLevel(bilinearClamp, uv + float3(offset.x, offset.y, 0.0), bindData.mipBase);
	}
	
	sourceSample *= 0.25;
	
	if (!bindData.array)
	{
		RWTexture2D<float4> outputMip0 = ResourceDescriptorHeap[bindData.outputTextureIndices[0]];
		outputMip0[input.dispatchId.xy] = SRGBAdjustedColor(sourceSample);
	}
	
	else
	{
		RWTexture2DArray<float4> outputMip0 = ResourceDescriptorHeap[bindData.outputTextureIndices[0]];
		outputMip0[uint3(input.dispatchId.xy, bindData.layer)] = SRGBAdjustedColor(sourceSample);
	}
	
	if (bindData.mipCount == 1)
	{
		return;
	}
	
	StoreGroupColor(input.groupIndex, sourceSample);
	
	GroupMemoryBarrierWithGroupSync();
	
	// X and Y are even.
	if ((input.groupIndex & 0x9) == 0)
	{
		sourceSample += LoadGroupColor(input.groupIndex + 0x01);
		sourceSample += LoadGroupColor(input.groupIndex + 0x08);
		sourceSample += LoadGroupColor(input.groupIndex + 0x09);
		
		sourceSample *= 0.25;
		
		if (!bindData.array)
		{
			RWTexture2D<float4> outputMip1 = ResourceDescriptorHeap[bindData.outputTextureIndices[1]];
			outputMip1[input.dispatchId.xy / 2] = SRGBAdjustedColor(sourceSample);
		}
		
		else
		{
			RWTexture2DArray<float4> outputMip1 = ResourceDescriptorHeap[bindData.outputTextureIndices[1]];
			outputMip1[uint3(input.dispatchId.xy / 2, bindData.layer)] = SRGBAdjustedColor(sourceSample);
		}

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
		sourceSample += LoadGroupColor(input.groupIndex + 0x02);
		sourceSample += LoadGroupColor(input.groupIndex + 0x10);
		sourceSample += LoadGroupColor(input.groupIndex + 0x12);
		
		sourceSample *= 0.25;
		
		if (!bindData.array)
		{
			RWTexture2D<float4> outputMip2 = ResourceDescriptorHeap[bindData.outputTextureIndices[2]];
			outputMip2[input.dispatchId.xy / 4] = SRGBAdjustedColor(sourceSample);
		}
		
		else
		{
			RWTexture2DArray<float4> outputMip2 = ResourceDescriptorHeap[bindData.outputTextureIndices[2]];
			outputMip2[uint3(input.dispatchId.xy / 4, bindData.layer)] = SRGBAdjustedColor(sourceSample);
		}
		
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
		sourceSample += LoadGroupColor(input.groupIndex + 0x04);
		sourceSample += LoadGroupColor(input.groupIndex + 0x20);
		sourceSample += LoadGroupColor(input.groupIndex + 0x24);
		
		sourceSample *= 0.25;
		
		if (!bindData.array)
		{
			RWTexture2D<float4> outputMip3 = ResourceDescriptorHeap[bindData.outputTextureIndices[3]];
			outputMip3[input.dispatchId.xy / 8] = SRGBAdjustedColor(sourceSample);
		}
		
		else
		{
			RWTexture2DArray<float4> outputMip3 = ResourceDescriptorHeap[bindData.outputTextureIndices[3]];
			outputMip3[uint3(input.dispatchId.xy / 8, bindData.layer)] = SRGBAdjustedColor(sourceSample);
		}
	}
}