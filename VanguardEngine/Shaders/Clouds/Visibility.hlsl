// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Constants.hlsli"
#include "Camera.hlsli"
#include "Reprojection.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Clouds/Core.hlsli"

struct BindData
{
	uint outputTexture;
	uint weatherTexture;
	uint baseShapeNoiseTexture;
	uint cameraBuffer;
	uint cameraIndex;
	float solarZenithAngle;
	uint timeSlice;
	uint geometryDepthTexture;
	uint blueNoiseTexture;
	uint atmosphereIrradianceBuffer;
	float2 wind;
	float time;
	uint2 upscaledResolution;
	uint accelerationStructure;
};

ConstantBuffer<BindData> bindData : register(b0);

// Returns (shadowStart, shadowLength) describing a single contiguous shadow segment along the camera view ray, in
// kilometers.
float2 RayMarch(Camera camera, float2 baseUv, float2 jitteredUv, uint width, uint height)
{
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 rayDirection = ComputeRayDirection(camera, jitteredUv);

	Texture3D<float> baseShapeNoiseTexture = ResourceDescriptorHeap[bindData.baseShapeNoiseTexture];
	Texture3D<float> detailShapeNoiseTexture;  // Null texture (visibility always marches at low detail).
	Texture3D<float4> curlNoiseTexture;  // Null texture (visibility always marches at low detail).
	StructuredBuffer<float3> atmosphereIrradiance = ResourceDescriptorHeap[bindData.atmosphereIrradianceBuffer];
	Texture2D<float3> weatherTexture = ResourceDescriptorHeap[bindData.weatherTexture];
	Texture2D<float> geometryDepthTexture = ResourceDescriptorHeap[bindData.geometryDepthTexture];
	Texture2D<float> blueNoiseTexture = ResourceDescriptorHeap[bindData.blueNoiseTexture];
	RaytracingAccelerationStructure accelerationStructure = ResourceDescriptorHeap[bindData.accelerationStructure];

	const float planetRadius = 6360.0;  // #TODO: Get from atmosphere data.
	// How dense the cloud media is. Use a fairly high density so that ray marches early out quickly rather
	// than stepping through lots of semi translucent clouds.
	const float densityMultiplier = 2.8f;

	float dist = 0.f;
	float3 origin = ComputeAtmosphereCameraPosition(camera);

	float marchStart = 0;
	float marchEnd;

	float2 topBoundaryIntersect;
	if (RaySphereIntersection(origin, rayDirection, planetCenter, planetRadius + cloudLayerTop, topBoundaryIntersect))
	{
		marchEnd = topBoundaryIntersect.y;
	}
	else
	{
		// Outside of the cloud layer.
		return float2(0, 0);
	}

	// Stop short if we hit the planet.
	float2 planetIntersect;
	if (RaySphereIntersection(origin, rayDirection, planetCenter, planetRadius, planetIntersect))
	{
		marchEnd = min(marchEnd, planetIntersect.x);
	}

	// Limit the march distance. Far away clouds won't meaningfully contribute shadow and are simply too expensive to march to.
	marchEnd = clamp(marchEnd, 0, 50);

	// Early out of the march if we hit opaque geometry.
	// Using the base UV instead of jittered provides slightly better edges around geometry.
	float geometryDepth = geometryDepthTexture.Sample(bilinearClamp, baseUv);
	geometryDepth = LinearizeDepth(camera, geometryDepth) * camera.farPlane;
	if (geometryDepth < camera.farPlane)
	{
		geometryDepth *= 0.001;  // Meters to kilometers.
		marchEnd = min(marchEnd, geometryDepth);
	}

	// #TODO: early out if we hit a cloud in screenspace too!

	if (marchEnd <= marchStart)
	{
		return float2(0, 0);
	}
	
	uint blueNoiseWidth, blueNoiseHeight;
	blueNoiseTexture.GetDimensions(blueNoiseWidth, blueNoiseHeight);
	const float upscaleResolutionMultiplier = 4.f;
	// Sample blue noise at one pixel per upscaled sample, so scale the coordinates by the resolution scale.
	float2 blueNoiseSamplePos = jitteredUv * uint2(width, height) * upscaleResolutionMultiplier;
	blueNoiseSamplePos = blueNoiseSamplePos / float2(blueNoiseWidth, blueNoiseHeight);
	float rayOffset = blueNoiseTexture.Sample(pointWrap, blueNoiseSamplePos);
	float jitter = saturate(rayOffset);  // [0, 1]

	float stepSize = (marchEnd - marchStart) / 20;
	dist += jitter * stepSize;

	// Precompute the noise kernel once for this pixel.
	ComputeNoiseKernel(sunDirection);
	
	float totalShadow = 0.f;
	// First moment (distance-weighted shadow). totalShadowWeighted / totalShadow gives the centroid distance of the
	// shadow distribution along the ray. With this, we can express the shadow as a single contiguous segment
	// centered at the centroid, which lets the atmosphere model omit in-scattering at the correct location along
	// the view ray rather than always at one end. See atmosphere shadow-segment formulation for the consumer side.
	float totalShadowWeighted = 0.f;
	float accumulatedTransmittance = 1.f;  // Used for early out once the ray is fully shadowed
#ifdef CLOUDS_DEBUG_MARCHCOUNT
	int totalSteps = 0;
#endif

	while (dist < marchEnd)
	{
		float3 position = origin + rayDirection * dist;
		
#ifdef VISIBILITY_ENABLE_GEOMETRY

		// During the clouds visibility march, also test geometry. This isn't an amazing solution, but it's
		// what works with the Bruneton atmosphere model. Newer models decouple visibility from the precomputed
		// stage, so froxels can be used. In this model, we have to approximate a shadow segment instead.
		// Eventually this whole shader should likely not be a screen space march, and should instead inject
		// into froxels.
			
		RayDesc shadowRay;
		shadowRay.Origin = position * 1000.f; // Kilometers to meters.
		shadowRay.Direction = sunDirection;
		shadowRay.TMin = 0.1f;  // Small bias against acne from samples landing just above the depth surface.
		shadowRay.TMax = 100000.f;

		RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
		query.TraceRayInline(accelerationStructure, RAY_FLAG_NONE, 0xFF, shadowRay);
		query.Proceed();

		if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		{
			// This step is fully shadowed, don't bother with cloud march.
			totalShadow += stepSize;
			totalShadowWeighted += stepSize * dist;

			dist += stepSize;
			continue;
		}

#endif  // VISIBILITY_ENABLE_GEOMETRY
		
#ifdef VISIBILITY_ENABLE_CLOUDS

		float localMarchStart = 0.f;  // Start at the sample point
		float localMarchEnd = 0.f;
		
		// Local march within the cloud layer boundary.
		float2 topBoundaryIntersect;
		if (RaySphereIntersection(position, sunDirection, planetCenter, planetRadius + cloudLayerTop, topBoundaryIntersect))
		{
			localMarchEnd = topBoundaryIntersect.y;

			float2 bottomBoundaryIntersect;
			if (RaySphereIntersection(position, sunDirection, planetCenter, planetRadius + cloudLayerBottom, bottomBoundaryIntersect))
			{
				float top = all(topBoundaryIntersect > 0) ? min(topBoundaryIntersect.x, topBoundaryIntersect.y) : max(topBoundaryIntersect.x, topBoundaryIntersect.y);
				float bottom = all(bottomBoundaryIntersect > 0) ? min(bottomBoundaryIntersect.x, bottomBoundaryIntersect.y) : max(bottomBoundaryIntersect.x, bottomBoundaryIntersect.y);
				if (all(bottomBoundaryIntersect > 0))
					top = max(0, min(topBoundaryIntersect.x, topBoundaryIntersect.y));
				localMarchStart = min(bottom, top);
				localMarchEnd = max(bottom, top);
			}
		}
		
		// Ignore any gap, only affects far horizon clouds so we don't care here.
		float gapStart = 0.f;
		float gapEnd = 0.f;
		
		// March towards the sun.
		float3 scatteredLuminance;
		float transmittance;
		float depth;  // Kilometers.
#ifdef CLOUDS_DEBUG_MARCHCOUNT
		int stepCount = RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture,
			position, sunDirection, 0.f, localMarchStart, localMarchEnd, gapStart, gapEnd, sunDirection, bindData.wind, bindData.time,
			densityMultiplier, scatteredLuminance, transmittance, depth);
		totalSteps += stepCount;
#else
		RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture,
			position, sunDirection, 0.f, localMarchStart, localMarchEnd, gapStart, gapEnd, sunDirection, bindData.wind, bindData.time,
			densityMultiplier, scatteredLuminance, transmittance, depth);
#endif
		
		// Use the atmosphere's transmittance for natural shadow attenuation.
		if (transmittance < 1.f)
		{
			const float contribution = stepSize * (1.f - transmittance);
			totalShadow += contribution;
			totalShadowWeighted += contribution * dist;  // First moment for centroid extraction.
			accumulatedTransmittance *= transmittance;
		}

#endif  // VISIBILITY_ENABLE_CLOUDS

		// Skip the remaining samples once the ray is essentially fully shadowed.
		if (accumulatedTransmittance < 0.01f)
			break;

		dist += stepSize;
	}

#ifdef CLOUDS_DEBUG_MARCHCOUNT
	return float2(totalSteps, 0);
#else
	// Convert the moments to a contiguous shadow segment: centered at the centroid distance with length equal to
	// the integrated shadow contribution. This collapses the real (potentially multi-modal) shadow distribution to
	// a single block, but for typical cloud scenes the dominant cloud accounts for nearly all of the shadow and
	// the centroid approximation is close enough.
	const float shadowLength = totalShadow;
	float shadowStart = 0.f;
	if (totalShadow > 0.0001f)
	{
		const float shadowCentroid = totalShadowWeighted / totalShadow;
		shadowStart = max(shadowCentroid - shadowLength * 0.5f, 0.f);
	}
	return float2(shadowStart, shadowLength);
#endif
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];

	uint width, height;
	outputTexture.GetDimensions(width, height);

	if (dispatchId.x >= width || dispatchId.y >= height)
		return;

	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	float2 uv = (dispatchId.xy + 0.5.xx) / float2(width, height);
	// Get the UV coordinates that are top-left aligned.
	float2 alignedUv = floor(uv * uint2(width, height)) / float2(width, height);
	// Jitter the UV coordinates for temporal accumulation.
	float2 jitteredUv = JitterUv(alignedUv, bindData.upscaledResolution, bindData.timeSlice);

	float2 shadowSegment = RayMarch(camera, uv, jitteredUv, width, height);

	outputTexture[dispatchId.xy] = shadowSegment;
}