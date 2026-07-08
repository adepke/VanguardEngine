// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Filtering.hlsli"

// Bilateral upsample of the accumulated cloud render to full resolution. Purely spatial - all temporal
// logic lives in the accumulation pass, so every full res pixel gets a full-coverage estimate every
// frame and no pixel's freshness depends on jitter alignment (which produced the legacy block artifacts).

struct BindData
{
	uint cameraBuffer;
	uint cameraIndex;
	uint geometryDepthTexture;  // Full resolution raw reversed-Z.
	uint geometryDepthMinMaxTexture;  // Low resolution (min, max) reversed-Z bounds.
	uint accumulatedScatteringTransmittanceTexture;
	uint accumulatedDepthTexture;  // Inverse kilometers.
	uint accumulatedVisibilityTexture;
	uint outputScatteringTransmittanceTexture;
	uint outputDepthTexture;  // Kilometers.
	uint outputVisibilityTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

// Falloff of the geometry depth similarity weight; higher separates silhouettes more aggressively.
static const float depthSimilarityFalloff = 24.f;

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	Texture2D<float> geometryDepthTexture = ResourceDescriptorHeap[bindData.geometryDepthTexture];
	Texture2D<float2> geometryDepthMinMaxTexture = ResourceDescriptorHeap[bindData.geometryDepthMinMaxTexture];
	Texture2D<float4> accumulatedScatTransTexture = ResourceDescriptorHeap[bindData.accumulatedScatteringTransmittanceTexture];
	Texture2D<float> accumulatedDepthTexture = ResourceDescriptorHeap[bindData.accumulatedDepthTexture];
	Texture2D<float2> accumulatedVisibilityTexture = ResourceDescriptorHeap[bindData.accumulatedVisibilityTexture];
	RWTexture2D<float4> outputScatTransTexture = ResourceDescriptorHeap[bindData.outputScatteringTransmittanceTexture];
	RWTexture2D<float> outputDepthTexture = ResourceDescriptorHeap[bindData.outputDepthTexture];
	RWTexture2D<float2> outputVisibilityTexture = ResourceDescriptorHeap[bindData.outputVisibilityTexture];

	uint width, height;
	outputScatTransTexture.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	uint lowWidth, lowHeight;
	accumulatedScatTransTexture.GetDimensions(lowWidth, lowHeight);

	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	const float2 uv = (dispatchId.xy + 0.5.xx) / float2(width, height);
	const float geometryDepth = LinearizeDepth(camera, geometryDepthTexture[dispatchId.xy]) * camera.farPlane;

	// Bilateral 2x2 tap upsample: bilinear weights modulated by geometry depth similarity, where each
	// low res tap's (min, max) raw depth bounds describe the range of full res depths it represents.
	// Taps whose footprint cannot contain this pixel's depth get strongly downweighted, preventing
	// cloud/sky data from bleeding across geometry silhouettes. All channels share identical weights so
	// they never desync.
	const float2 lowPosition = uv * float2(lowWidth, lowHeight) - 0.5.xx;
	const int2 basePixel = int2(floor(lowPosition));
	const float2 fraction = lowPosition - basePixel;

	const float bilinearWeights[4] = {
		(1.f - fraction.x) * (1.f - fraction.y),
		fraction.x * (1.f - fraction.y),
		(1.f - fraction.x) * fraction.y,
		fraction.x * fraction.y
	};
	const int2 tapOffsets[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };

	float4 scatTransSum = 0.xxxx;
	float inverseDepthSum = 0.f;
	float depthWeightSum = 0.f;
	float2 visibilitySum = 0.xx;
	float weightSum = 0.f;

	float bestSimilarity = -1.f;
	float minSimilarity = 1.f;
	int2 bestCoord = int2(0, 0);

	[unroll]
	for (int tap = 0; tap < 4; ++tap)
	{
		const int2 coord = clamp(basePixel + tapOffsets[tap], int2(0, 0), int2(lowWidth - 1, lowHeight - 1));
		const float2 depthBounds = geometryDepthMinMaxTexture[coord];
		// Reversed-Z: the raw max channel is the nearest linear depth, raw min is the farthest.
		const float nearest = LinearizeDepth(camera, depthBounds.y) * camera.farPlane;
		const float farthest = LinearizeDepth(camera, depthBounds.x) * camera.farPlane;

		float similarity = 1.f;
		if (geometryDepth < nearest)
			similarity = exp(-depthSimilarityFalloff * (nearest - geometryDepth) / geometryDepth);
		else if (geometryDepth > farthest)
			similarity = exp(-depthSimilarityFalloff * (geometryDepth - farthest) / geometryDepth);

		if (similarity > bestSimilarity)
		{
			bestSimilarity = similarity;
			bestCoord = coord;
		}
		minSimilarity = min(minSimilarity, similarity);

		const float weight = bilinearWeights[tap] * similarity;
		const float4 tapScatTrans = accumulatedScatTransTexture[coord];
		scatTransSum += tapScatTrans * weight;
		visibilitySum += accumulatedVisibilityTexture[coord] * weight;
		weightSum += weight;

		// Depth is additionally weighted by the tap's cloud opacity: blending depth between cloud and
		// empty (sentinel) taps writes phantom intermediate depths into the ring of pixels around every
		// cloud edge, which the compose pass turns into a blue aerial perspective halo. The weight must
		// be continuous in opacity - a thresholded weight (saturate(x * 8)) zeroed the depth of thin
		// wisps, and since compose only composites cloud scattering when depth is valid, whole wisps
		// dropped out of the image as flickering low res blocks.
		const float depthWeight = weight * (1.f - tapScatTrans.a);
		inverseDepthSum += accumulatedDepthTexture[coord] * depthWeight;
		depthWeightSum += depthWeight;
	}

	float4 scatTrans;
	float inverseDepth;
	float2 visibility;
	if (weightSum > 0.0001f)
	{
		scatTrans = scatTransSum / weightSum;
		visibility = visibilitySum / weightSum;
	}
	else
	{
		// Degenerate weights (thin geometry whose depth no tap footprint contains): fall back to the
		// most depth-similar tap instead of dividing by ~zero.
		scatTrans = accumulatedScatTransTexture[bestCoord];
		visibility = accumulatedVisibilityTexture[bestCoord];
	}

	// When the footprint contains no geometry discontinuity (the common case, and always true against
	// sky), upgrade the scattering estimate from bilinear to Catmull-Rom bicubic: a 4x bilinear upscale
	// is a strong low pass and was a major contributor to the overall blurriness. Negative lobes can
	// overshoot, so clamp back to the physical range.
	if (minSimilarity > 0.99f)
	{
		float4 bicubic = SampleCatmullRom(accumulatedScatTransTexture, uv, float2(lowWidth, lowHeight));
		scatTrans = float4(max(bicubic.rgb, 0.xxx), clamp(bicubic.a, 0.f, 1.f));
	}

	// No meaningful cloud under any contributing tap: no cloud depth (sentinel).
	inverseDepth = depthWeightSum > 0.0001f ? inverseDepthSum / depthWeightSum : 0.f;

	outputScatTransTexture[dispatchId.xy] = scatTrans;
	// Inverse kilometers back to kilometers; ~0 maps to the no-cloud far sentinel (1e6 km).
	outputDepthTexture[dispatchId.xy] = 1.f / max(inverseDepth, 0.000001f);
	outputVisibilityTexture[dispatchId.xy] = visibility;
}
