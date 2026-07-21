// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Utils/OctahedralNormals.hlsli"

// Edge-aware spatial filter for ray traced shadows.
// Texture format is: (visibility, viewSpaceZ)

struct BindData
{
	uint inputTexture;
	uint normalTexture;
	uint outputTexture;
	uint filterRadius;  // pixels.
	uint2 outputResolution;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	if (dispatchId.x >= bindData.outputResolution.x || dispatchId.y >= bindData.outputResolution.y)
	{
		return;
	}

	Texture2D<float2> inputTexture = ResourceDescriptorHeap[bindData.inputTexture];
	Texture2D<float2> normalTexture = ResourceDescriptorHeap[bindData.normalTexture];
	RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];

	const float2 center = inputTexture.Load(int3(dispatchId.xy, 0));

	// Sky pixels don't accumulate.
	if (center.y == 0.f)
	{
		outputTexture[dispatchId.xy] = float2(center.x, 0.f);
		return;
	}

	const float3 centerNormal = DecodeNormalOctahedral(normalTexture.Load(int3(dispatchId.xy, 0)));

	float accumulated = 0.f;
	float totalWeight = 0.f;

	const int radius = int(bindData.filterRadius);
	for (int y = -radius; y <= radius; ++y)
	{
		for (int x = -radius; x <= radius; ++x)
		{
			const int2 tap = int2(dispatchId.xy) + int2(x, y);
			if (any(tap < 0) || any(tap >= int2(bindData.outputResolution)))
			{
				continue;
			}

			const float2 sample = inputTexture.Load(int3(tap, 0));
			const float3 sampleNormal = DecodeNormalOctahedral(normalTexture.Load(int3(tap, 0)));

			// Depth edge stopping: relative view depth difference.
			const float depthWeight = exp(-abs(sample.y - center.y) / (0.03f * abs(center.y) + 1e-4f));
			// Normal edge stopping.
			const float normalWeight = pow(saturate(dot(centerNormal, sampleNormal)), 32.f);

			const float weight = depthWeight * normalWeight;
			accumulated += sample.x * weight;
			totalWeight += weight;
		}
	}

	const float visibility = totalWeight > 0.f ? accumulated / totalWeight : center.x;

	outputTexture[dispatchId.xy] = float2(visibility, center.y);
}
