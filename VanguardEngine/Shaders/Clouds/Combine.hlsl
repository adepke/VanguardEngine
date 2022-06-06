// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"

struct BindData
{
	uint cloudsTexture;
	uint outputTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	Texture2D<float4> cloudsTexture = ResourceDescriptorHeap[bindData.cloudsTexture];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];

	uint width, height;
	outputTexture.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	float4 source = cloudsTexture[dispatchId.xy];
	float4 dest = outputTexture[dispatchId.xy];
	outputTexture[dispatchId.xy] = float4((source.rgb * source.a) + (dest.rgb * (1 - source.a)), 1);
}