// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Noise.hlsli"

struct BindData
{
	uint outputTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

float RemapRange(float value, float inMin, float inMax, float outMin, float outMax)
{
	return outMin + (((value - inMin) / (inMax - inMin)) * (outMax - outMin));
}

static const uint baseSize = 128;
static const uint detailSize = 32;

[RootSignature(RS)]
[numthreads(baseSize, 1, 1)]
void BaseShapeMain(uint3 dispatchId : SV_DispatchThreadID)
{
	static const float noiseFrequencies[] = { 2, 8, 14, 20, 26, 32 };

	RWTexture3D<float> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];

	for (uint i = 0; i < baseSize; ++i)
	{
		for (uint j = 0; j < baseSize; ++j)
		{
			float3 coord = float3(dispatchId.x, i, j) * (1.0 / baseSize);

			static const float frequency = 8;
			static const uint octaveCount = 3;

			float perlin = PerlinNoise3D(coord, frequency, octaveCount);
			float perlinWorley = 0.f;
			{
				const float cellCount = 4;
				const float worleyNoise0 = (1.0 - WorleyNoise3D(coord, cellCount * noiseFrequencies[0]));
				const float worleyNoise1 = (1.0 - WorleyNoise3D(coord, cellCount * noiseFrequencies[1]));
				const float worleyNoise2 = (1.0 - WorleyNoise3D(coord, cellCount * noiseFrequencies[2]));

				float worleyFBM = worleyNoise0 * 0.625f + worleyNoise1 * 0.25f + worleyNoise2 * 0.125f;
				perlinWorley = RemapRange(perlin, 0.f, 1.f, worleyFBM, 1.f);
			}

			float worleyFBM0;
			float worleyFBM1;
			float worleyFBM2;
			{
				const float cellCount = 4;
				const float worleyNoise1 = (1.0 - WorleyNoise3D(coord, cellCount * 2));
				const float worleyNoise2 = (1.0 - WorleyNoise3D(coord, cellCount * 4));
				const float worleyNoise3 = (1.0 - WorleyNoise3D(coord, cellCount * 8));
				const float worleyNoise4 = (1.0 - WorleyNoise3D(coord, cellCount * 16));

				worleyFBM0 = worleyNoise1 * 0.625f + worleyNoise2 * 0.25f + worleyNoise3 * 0.125f;
				worleyFBM1 = worleyNoise2 * 0.625f + worleyNoise3 * 0.25f + worleyNoise4 * 0.125f;
				worleyFBM2 = worleyNoise3 * 0.75f + worleyNoise4 * 0.25f;
			}

			float value = 0.f;
			const float lowFreqFBM = worleyFBM0 * 0.625f + worleyFBM1 * 0.25f + worleyFBM2 * 0.125f;
			const float baseCloud = perlinWorley;
			value = RemapRange(baseCloud, -(1.0 - lowFreqFBM), 1.0, 0.0, 1.0);
			value = RemapRange(value, 0.3, 1, 0, 1);  // Improve distribution.
			value = saturate(value);
			outputTexture[uint3(dispatchId.x, i, j)] = value;
		}
	}
}

[RootSignature(RS)]
[numthreads(detailSize, 1, 1)]
void DetailShapeMain(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture3D<float> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];

	for (uint i = 0; i < detailSize; ++i)
	{
		for (uint j = 0; j < detailSize; ++j)
		{
			float3 coord = float3(dispatchId.x, i, j) * (1.0 / detailSize);

			const float cellCount = 2;
			const float worleyNoise0 = (1.0 - WorleyNoise3D(coord, cellCount * 1));
			const float worleyNoise1 = (1.0 - WorleyNoise3D(coord, cellCount * 2));
			const float worleyNoise2 = (1.0 - WorleyNoise3D(coord, cellCount * 4));
			const float worleyNoise3 = (1.0 - WorleyNoise3D(coord, cellCount * 8));
			const float worleyFBM0 = worleyNoise0 * 0.625f + worleyNoise1 * 0.25f + worleyNoise2 * 0.125f;
			const float worleyFBM1 = worleyNoise1 * 0.625f + worleyNoise2 * 0.25f + worleyNoise3 * 0.125f;
			const float worleyFBM2 = worleyNoise2 * 0.75f + worleyNoise3 * 0.25f;

			float value = worleyFBM0 * 0.625f + worleyFBM1 * 0.25f + worleyFBM2 * 0.125f;
			outputTexture[uint3(dispatchId.x, i, j)] = value;
		}
	}
}