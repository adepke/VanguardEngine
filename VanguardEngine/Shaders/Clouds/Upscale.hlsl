// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Reprojection.hlsli"
#include "Volumetrics/VisibilityMoments.hlsli"

struct BindData
{
	uint cameraBuffer;
	uint cameraIndex;
	uint timeSlice;
	uint geometryDepthTexture;
	uint newScatteringTransmittanceTexture;
	uint newDepthTexture;
	uint newVisibilityTexture;
	uint oldScatteringTransmittanceTexture;
	uint oldDepthTexture;
	uint oldVisibilityTexture;
	uint outputScatteringTransmittanceTexture;
	uint outputDepthTexture;
	uint outputVisibilityTexture;
};

ConstantBuffer<BindData> bindData : register(b0);

// 3x3 neighborhood clamp filter by Playdead Games
// From: https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
template <typename T, typename U>
T NeighborhoodClampFilter(T source, U targetTexture, uint2 location)
{
	const T a = targetTexture[location + int2(-1, -1)];
	const T b = targetTexture[location + int2(-1, 0)];
	const T c = targetTexture[location + int2(-1, 1)];
	const T d = targetTexture[location + int2(0, -1)];
	const T e = targetTexture[location + int2(0, 0)];
	const T f = targetTexture[location + int2(0, 1)];
	const T g = targetTexture[location + int2(1, -1)];
	const T h = targetTexture[location + int2(1, 0)];
	const T i = targetTexture[location + int2(1, 1)];
	
	// Compute min and max bounds.
	const T minColor = min(min(min(min(min(min(min(min(a, b), c), d), e), f), g), h), i);
	const T maxColor = max(max(max(max(max(max(max(max(a, b), c), d), e), f), g), h), i);
	
	// #TODO: in the future, consider AABB clipping to reduce color grouping.
	return clamp(source, minColor, maxColor);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	// Low resolution renders from the current frame.
	Texture2D<float4> newScatTransTexture = ResourceDescriptorHeap[bindData.newScatteringTransmittanceTexture];
	Texture2D<float> newDepthTexture = ResourceDescriptorHeap[bindData.newDepthTexture];
	Texture2D<float2> newVisibilityTexture = ResourceDescriptorHeap[bindData.newVisibilityTexture];
	// Upscaled renders from previous frames.
	Texture2D<float4> oldScatTransTexture = ResourceDescriptorHeap[bindData.oldScatteringTransmittanceTexture];
	Texture2D<float> oldDepthTexture = ResourceDescriptorHeap[bindData.oldDepthTexture];
	Texture2D<float2> oldVisibilityTexture = ResourceDescriptorHeap[bindData.oldVisibilityTexture];
	// Upscaled outputs for the current frame.
	RWTexture2D<float4> outputScatTransTexture = ResourceDescriptorHeap[bindData.outputScatteringTransmittanceTexture];
	RWTexture2D<float> outputDepthTexture = ResourceDescriptorHeap[bindData.outputDepthTexture];
	RWTexture2D<float2> outputVisibilityTexture = ResourceDescriptorHeap[bindData.outputVisibilityTexture];
	
	Texture2D<float> geometryDepthTexture = ResourceDescriptorHeap[bindData.geometryDepthTexture];
	
	uint width, height;
	outputScatTransTexture.GetDimensions(width, height);
	if (dispatchId.x >= width || dispatchId.y >= height)
		return;
	
	if (bindData.oldScatteringTransmittanceTexture == 0 || bindData.oldDepthTexture == 0)
		return;
	
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	
	// Divide by 4 with truncation, every pixel must map 1-1 exactly. If the low res texture dimensions are smaller than
	// quarter resolution, then there will be no data in the bottom right. Cannot do an interpolated mapping with UV's
	// as this causes banding to appear.
	uint2 lowResSampleCoords = uint2(dispatchId.xy / 4.xx);
	
	float4 newScatTrans = newScatTransTexture[lowResSampleCoords];
	float newDepth = newDepthTexture[lowResSampleCoords];
	float2 newVisibility;
	
	// UV coordinates are centered on the middle of the pixel, not the aligned corner.
	float2 newUv = (dispatchId.xy + 0.5.xx) / float2(width, height);
	float2 oldUv = ReprojectUv(camera, newUv, newDepth);
	
	// Perform a bilinear blur while sampling the visibility texture to denoise a bit.
	newVisibility = newVisibilityTexture.Sample(bilinearClamp, newUv);
	
	// With the reprojected UV coordinates, sample the last frames upscaled data.
	float4 oldScatTrans = oldScatTransTexture.Sample(pointClamp, oldUv);
	float oldDepth = oldDepthTexture.Sample(pointClamp, oldUv);
	
	// Run through a series of history rejection tests to discard bad history.
	bool reject = false;
	
	if (any(oldUv > 1.f) || any(oldUv < 0.f))
	{
		reject = true;
	}
	
	// Occlusion detection against geometry.
	float geometryDepth = geometryDepthTexture.Sample(bilinearClamp, newUv);
	geometryDepth = LinearizeDepth(camera, geometryDepth) * camera.farPlane;
	if (geometryDepth < camera.farPlane && newDepth < 100000.f)
	{
		// Kilometers to meters.
		if (geometryDepth < newDepth * 1000.f)
		{
			reject = true;
		}
	}

	// With the new frame data and the reprojected history, produce a new upscaled render for the current frame.
	
	// Assume only sampling from the current frame render.
	float4 finalScatTrans = newScatTrans;
	float finalDepth = newDepth;
	float2 finalVisibility = newVisibility;
	
	float blendWeight = JitterAlignedPixel(newUv, uint2(width, height), bindData.timeSlice);

	// No reprojection if the camera didn't move.
	float2 uvDelta = newUv - oldUv;
	const bool stationaryCamera = (uvDelta.x * uvDelta.x + uvDelta.y * uvDelta.y < 0.000001) &&
		all(abs(camera.lastFramePosition.xyz - camera.position.xyz) < 0.01);

	if (!reject)
	{
		// Apply neighborhood clipping.
		// See: https://gdcvault.com/play/1022970/Temporal-Reprojection-Anti-Aliasing-in
		float4 clippedScatTrans = NeighborhoodClampFilter(oldScatTrans, newScatTransTexture, lowResSampleCoords);
		float clippedDepth = NeighborhoodClampFilter(oldDepth, newDepthTexture, lowResSampleCoords);
		
		// Unfortunately, just using the neighborhood clamped sample is not so simple.
		// Consider the situation of a small patch of clouds very far away, such that it only occupies a couple of subpixels
		// in the low resolution output. Since the ray directions get jittered, some of the upscaled pixels will sample
		// that region of clouds, while others will not, resulting in bits of cloud that flicker, as they only appear in
		// in some time slices. For the neighborhood clamping to work properly here, it would have to sample the middle
		// subpixel to fetch the "real" information for that region, but this would require doubling the cloud rendering!
		// As a result, hack it a bit and only apply the neighborhood clipping while movement is happening to hide this
		// temporal artifacts, but when the camera is still, just do a simple pass through. No need to neighborhood clip
		// during no motion anyways.
		float4 correctedScatTrans = clippedScatTrans;
		float correctedDepth = clippedDepth;
		if (stationaryCamera)
		{
			correctedScatTrans = oldScatTrans;
			correctedDepth = oldDepth;
		}
		
		// Finally, blend together. The clipping work may get entirely discarded if the pixel is not jitter aligned,
		// but oh well.
		finalScatTrans = lerp(newScatTrans, correctedScatTrans, blendWeight);
		finalDepth = lerp(newDepth, correctedDepth, blendWeight);
	}
	
	// Visibility upscale is a bit different, it works in shadow moment space.
	// Experimenting with exponential moving average to help smooth noise, instead of jitter-aligned
	// replacement + reprojection that the clouds use.

	// Representative depth of the shadow signal, for reprojection. In kilometers.
	float visibilityDepth = visibilityMarchMax;
	const float newCentroid = VisibilityMomentsCentroid(newVisibility);
	if (newCentroid > 0.f)
	{
		visibilityDepth = newCentroid;
	}
	else if (geometryDepth < camera.farPlane)
	{
		visibilityDepth = min(geometryDepth * 0.001f, visibilityMarchMax);  // Meters to kilometers.
	}

	float2 oldUvVisibility = ReprojectUv(camera, newUv, visibilityDepth);

	if (all(oldUvVisibility >= 0.f) && all(oldUvVisibility <= 1.f))
	{
		// Still bilinear blur. This should be correct to do since the visibility sample isn't a discrete segment.
		float2 oldVisibility = oldVisibilityTexture.Sample(bilinearClamp, oldUvVisibility);

		// Reproject the centroid from old frame to new frame.
		const float3 rayDirection = ComputeRayDirection(camera, newUv);
		const float3 featurePosition = camera.position.xyz + rayDirection * (visibilityDepth * 1000.f);  // Meters.
		const float oldDistance = length(featurePosition - camera.lastFramePosition.xyz) * 0.001f;  // Kilometers.
		oldVisibility.y *= visibilityDepth / max(oldDistance, 0.001f);
		// The centroid cannot lie past the march cap. With m1 normalized by the cap, that bound is just m0.
		oldVisibility.y = min(oldVisibility.y, oldVisibility.x);

		// Same clamp that the clouds use, same benefits and drawbacks.
		if (!stationaryCamera)
		{
			oldVisibility = NeighborhoodClampFilter(oldVisibility, newVisibilityTexture, lowResSampleCoords);
		}
		
		// EMA since the visibility march is a fairly low quality march with big steps. This works
		// better for visibility than it does for clouds since the visiblity contributes low frequency
		// signal that the eye won't really pick up on, as opposed to the very high fidelity clouds.
		const bool jitterAligned = blendWeight < 0.5f;
		const float historyWeight = jitterAligned ? 0.7f : 0.95f;
		finalVisibility = lerp(newVisibility, oldVisibility, historyWeight);
	}
	
	outputScatTransTexture[dispatchId.xy] = finalScatTrans;
	outputDepthTexture[dispatchId.xy] = finalDepth;
	outputVisibilityTexture[dispatchId.xy] = finalVisibility;
}
