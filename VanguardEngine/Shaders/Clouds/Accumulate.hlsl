// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Reprojection.hlsli"
#include "Filtering.hlsli"

// Temporal accumulation of the stochastic cloud render, at the cloud render resolution. Every pixel
// receives a fresh (jittered) sample every frame, so an exponential blend converges smoothly and
// uniformly - unlike the legacy interleaved reconstruction, where each full res pixel only refreshed
// once per 16-frame jitter cycle. History is reprojected with the cloud depth, variance clipped
// against the current frame's neighborhood distribution, and rejected on disocclusion.

struct BindData
{
	uint cameraBuffer;
	uint cameraIndex;
	uint geometryDepthMinMaxTexture;
	uint newScatteringTransmittanceTexture;
	uint newDepthTexture;
	uint newVisibilityTexture;
	uint historyScatteringTransmittanceTexture;
	uint historyDepthTexture;
	uint historyVisibilityTexture;
	uint outputScatteringTransmittanceTexture;
	uint outputDepthTexture;
	uint outputVisibilityTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

// Exponential blend factors: the fraction of the new frame integrated each frame. Higher responds
// faster but retains more sampling noise. Visibility is a very low frequency signal, so it affords
// heavier smoothing.
static const float scatteringBlendFactor = 0.15f;
static const float visibilityBlendFactor = 0.08f;
// Variance clipping width in standard deviations. Smaller clips history more aggressively but
// flickers at noisy edges as the box tracks the per-frame neighborhood.
static const float varianceGamma = 1.5f;

// Cloud depth is accumulated in inverse kilometers: blending is stable across the no-cloud depth
// sentinel (1e6 km, which maps to ~0 inverse) where a linear average would produce nonsense
// intermediate depths. The upsample pass converts back to kilometers.
float InverseCloudDepth(float depthKm)
{
	return 1.f / max(depthKm, 0.01f);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	Texture2D<float2> geometryDepthMinMaxTexture = ResourceDescriptorHeap[bindData.geometryDepthMinMaxTexture];
	Texture2D<float4> newScatTransTexture = ResourceDescriptorHeap[bindData.newScatteringTransmittanceTexture];
	Texture2D<float> newDepthTexture = ResourceDescriptorHeap[bindData.newDepthTexture];
	Texture2D<float2> newVisibilityTexture = ResourceDescriptorHeap[bindData.newVisibilityTexture];
	RWTexture2D<float4> outputScatTransTexture = ResourceDescriptorHeap[bindData.outputScatteringTransmittanceTexture];
	RWTexture2D<float> outputDepthTexture = ResourceDescriptorHeap[bindData.outputDepthTexture];
	RWTexture2D<float2> outputVisibilityTexture = ResourceDescriptorHeap[bindData.outputVisibilityTexture];

	uint width, height;
	outputScatTransTexture.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	const int2 pixel = int2(dispatchId.xy);
	float4 newScatTrans = newScatTransTexture[pixel];
	float newDepth = newDepthTexture[pixel];  // Kilometers.
	float2 newVisibility = newVisibilityTexture[pixel];

	// Belt and braces alongside the NaN firewall in the render passes: a NaN entering the exponential
	// history would persist indefinitely.
	if (any(isnan(newScatTrans)) || isnan(newDepth))
	{
		newScatTrans = float4(0.f, 0.f, 0.f, 1.f);
		newDepth = 1000000.f;
	}
	if (any(isnan(newVisibility)))
		newVisibility = float2(0.f, 0.f);

	// First and second moments of the current frame's 3x3 neighborhood for variance clipping, simple
	// min/max bounds for the visibility clamp, and the closest cloud depth for reprojection dilation.
	float4 moment1 = 0.xxxx;
	float4 moment2 = 0.xxxx;
	float4 neighborhoodMin = float4(100000.f, 100000.f, 100000.f, 100000.f);
	float4 neighborhoodMax = float4(-100000.f, -100000.f, -100000.f, -100000.f);
	float2 visibilityMin = float2(10000.f, 10000.f);
	float2 visibilityMax = float2(-10000.f, -10000.f);
	float closestDepth = 1000000.f;
	[unroll]
	for (int dy = -1; dy <= 1; ++dy)
	{
		[unroll]
		for (int dx = -1; dx <= 1; ++dx)
		{
			const int2 coord = clamp(pixel + int2(dx, dy), int2(0, 0), int2(width - 1, height - 1));
			const float4 scatTransSample = newScatTransTexture[coord];
			moment1 += scatTransSample;
			moment2 += scatTransSample * scatTransSample;
			neighborhoodMin = min(neighborhoodMin, scatTransSample);
			neighborhoodMax = max(neighborhoodMax, scatTransSample);
			const float2 visibilitySample = newVisibilityTexture[coord];
			visibilityMin = min(visibilityMin, visibilitySample);
			visibilityMax = max(visibilityMax, visibilitySample);
			closestDepth = min(closestDepth, newDepthTexture[coord]);
		}
	}
	moment1 /= 9.f;
	const float4 sigma = sqrt(max(moment2 / 9.f - moment1 * moment1, 0.xxxx));
	// Intersect the variance box with the neighborhood's actual bounds: mu +- gamma*sigma is not
	// bounded by the sample range, so at noisy edges it dips below zero and clipped history came out
	// with negative scattering - rendered as a dark blue/purple rim around cloud edges after compose.
	// The intersection also keeps transmittance physical and the box tighter overall.
	const float4 boxMin = max(moment1 - varianceGamma * sigma, neighborhoodMin);
	const float4 boxMax = min(moment1 + varianceGamma * sigma, neighborhoodMax);

	// Defaults for invalid history: pass through the new frame. This also covers the first frame after
	// initialization or a reconstruction mode switch, when no history textures are bound.
	float4 outScatTrans = newScatTrans;
	float outInverseDepth = InverseCloudDepth(newDepth);
	float2 outVisibility = newVisibility;

	bool historyValid = bindData.historyScatteringTransmittanceTexture != 0 && bindData.historyDepthTexture != 0;

	const float2 uv = (dispatchId.xy + 0.5.xx) / float2(width, height);
	// Reproject at the closest cloud depth in the neighborhood rather than this pixel's own depth: at
	// cloud edges the per-pixel depth alternates between cloud and the no-cloud sentinel as jittered
	// samples hit and miss, which made the history fetch position flicker during translation. The
	// sentinel is also clamped so the reprojection point stays numerically sane.
	const float2 oldUv = ReprojectUv(camera, uv, min(closestDepth, 10000.f));

	// Inverted logic so NaN reprojections also fail validation (NaN comparisons are always false).
	if (!(all(oldUv >= 0.f) && all(oldUv <= 1.f)))
		historyValid = false;

	// Occlusion rejection, only when a real cloud sample exists and geometry now covers it. Testing the
	// no-cloud sentinel would reject every cloudless pixel in front of geometry and permanently disable
	// accumulation there.
	const float geometryFarthest = LinearizeDepth(camera, geometryDepthMinMaxTexture[pixel].x) * camera.farPlane;
	if (geometryFarthest < camera.farPlane && newDepth < 100000.f && geometryFarthest < newDepth * 1000.f)
		historyValid = false;

	if (historyValid)
	{
		Texture2D<float4> historyScatTransTexture = ResourceDescriptorHeap[bindData.historyScatteringTransmittanceTexture];
		Texture2D<float> historyDepthTexture = ResourceDescriptorHeap[bindData.historyDepthTexture];

		float4 historyScatTrans = SampleCatmullRom(historyScatTransTexture, oldUv, float2(width, height));
		float historyInverseDepth = historyDepthTexture.SampleLevel(bilinearClamp, oldUv, 0);  // Inverse kilometers.
		if (any(isnan(historyScatTrans)))
			historyScatTrans = newScatTrans;
		if (isnan(historyInverseDepth))
			historyInverseDepth = InverseCloudDepth(newDepth);

		// Always clamp history to the physical range (Catmull-Rom lobes can overshoot into negative
		// scattering, which composited as a dark rim), independent of the neighborhood clip below.
		historyScatTrans = float4(max(historyScatTrans.rgb, 0.xxx), saturate(historyScatTrans.a));

		// Neighborhood clipping is gated by camera translation. Clipping exists to bound parallax
		// ghosting, and only translation causes parallax: rotation reprojects exactly, and hard
		// invalidation (off-screen, occlusion) is handled by the explicit rejection tests above.
		// Clipping the static case is actively destructive with a stochastic estimator: an isolated
		// wisp smaller than the 3x3 neighborhood collapses the box to "empty" on frames where the
		// jittered march misses it, wiping the history so the wisp flickers in and out instead of
		// accumulating - the same failure the legacy path worked around by bypassing its clamp when
		// stationary.
		const float translation = length(camera.position.xyz - camera.lastFramePosition.xyz);  // Meters per frame.
		const float clipStrength = saturate(translation * 2.f);
		const float4 clippedHistory = lerp(historyScatTrans, clamp(historyScatTrans, boxMin, boxMax), clipStrength);
		const float clipDistance = length(historyScatTrans - clippedHistory);
		const float responsiveness = saturate(clipDistance / max(length(sigma) * 4.f, 0.0001f));
		// Cap the responsive boost well below 1: edge pixels clip a little every frame under the
		// stochastic jitter, and letting them jump straight to the raw new sample defeats the
		// accumulation exactly where smoothing is needed most (edge flicker).
		const float blendFactor = lerp(scatteringBlendFactor, 0.5f, responsiveness);

		outScatTrans = lerp(clippedHistory, newScatTrans, blendFactor);

		// Blend depth in inverse space, weighting the update by the new sample's opacity. An empty
		// sample (transmittance ~1) carries the meaningless sentinel depth - letting it drag history at
		// the full rate made cloud edges flicker as jittered samples alternated hit/miss. Empty samples
		// still drift the depth slowly, so stale depth decays away after a cloud dissipates.
		const float newInverseDepth = InverseCloudDepth(newDepth);
		const float newOpacity = 1.f - newScatTrans.a;
		const float depthBlendFactor = blendFactor * lerp(0.15f, 1.f, saturate(newOpacity * 8.f));
		outInverseDepth = lerp(historyInverseDepth, newInverseDepth, depthBlendFactor);

		// Once the accumulated cloud is essentially gone, its depth is meaningless - force the sentinel
		// immediately. A stale depth on an empty pixel makes the compose pass apply aerial perspective
		// for a segment containing no cloud, rendering a blue in-scatter halo that trails cloud edges
		// under camera motion. The threshold must sit at the very edge of empty: compose only composites
		// cloud scattering when depth is valid, so a looser threshold (0.99) visibly dropped thin wisps
		// out of the image entirely, as flickering low res blocks at cloud edges.
		if (outScatTrans.a > 0.999f)
			outInverseDepth = InverseCloudDepth(1000000.f);
	}

	// Visibility accumulates separately: reprojected at the shadow segment's centroid distance (a far
	// constant when there's no shadow), clamped to the neighborhood bounds, and blended slower. Both
	// channels are accumulated together - they're physically correlated and blending them independently
	// would let them desync visually.
	if (bindData.historyVisibilityTexture != 0)
	{
		const float visibilityDepth = newVisibility.y > 0.f ? newVisibility.x + 0.5f * newVisibility.y : 10000.f;
		const float2 oldUvVisibility = ReprojectUv(camera, uv, visibilityDepth);
		if (all(oldUvVisibility >= 0.f) && all(oldUvVisibility <= 1.f))
		{
			Texture2D<float2> historyVisibilityTexture = ResourceDescriptorHeap[bindData.historyVisibilityTexture];
			float2 historyVisibility = historyVisibilityTexture.SampleLevel(bilinearClamp, oldUvVisibility, 0);
			if (any(isnan(historyVisibility)))
				historyVisibility = newVisibility;
			historyVisibility = clamp(historyVisibility, visibilityMin, visibilityMax);
			outVisibility = lerp(historyVisibility, newVisibility, visibilityBlendFactor);
		}
	}

	outputScatTransTexture[dispatchId.xy] = outScatTrans;
	outputDepthTexture[dispatchId.xy] = outInverseDepth;
	outputVisibilityTexture[dispatchId.xy] = outVisibility;
}
