// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"

struct BindData
{
	uint depthTexture;
	uint outputTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

// Downsamples the full resolution geometry depth to the cloud render resolution, storing per-texel
// (min, max) raw reversed-Z bounds over the covered footprint. The min channel (farthest depth) drives
// the conservative raymarch early-out and history occlusion tests; the full interval drives the
// bilateral upsample's depth similarity weights.
[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	Texture2D<float> depthTexture = ResourceDescriptorHeap[bindData.depthTexture];
	RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];

	uint outputWidth, outputHeight;
	outputTexture.GetDimensions(outputWidth, outputHeight);
	if (dispatchId.x >= outputWidth || dispatchId.y >= outputHeight)
		return;

	uint inputWidth, inputHeight;
	depthTexture.GetDimensions(inputWidth, inputHeight);

	// Ceiling of the footprint so partial edge blocks are still fully covered.
	const uint2 blockSize = uint2((inputWidth + outputWidth - 1) / outputWidth, (inputHeight + outputHeight - 1) / outputHeight);
	const uint2 base = dispatchId.xy * blockSize;

	float minDepth = 1.f;
	float maxDepth = 0.f;
	for (uint y = 0; y < blockSize.y; ++y)
	{
		for (uint x = 0; x < blockSize.x; ++x)
		{
			const uint2 coord = min(base + uint2(x, y), uint2(inputWidth - 1, inputHeight - 1));
			const float depth = depthTexture[coord];
			minDepth = min(minDepth, depth);
			maxDepth = max(maxDepth, depth);
		}
	}

	outputTexture[dispatchId.xy] = float2(minDepth, maxDepth);
}
