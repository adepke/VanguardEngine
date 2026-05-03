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
	uint2 upscaledResolution;
	float2 wind;
	float time;
};

ConstantBuffer<BindData> bindData : register(b0);

float RayMarch(Camera camera, float2 baseUv, float2 jitteredUv, uint width, uint height)
{	
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 rayDirection = ComputeRayDirection(camera, jitteredUv);

	Texture3D<float> baseShapeNoiseTexture = ResourceDescriptorHeap[bindData.baseShapeNoiseTexture];
	Texture3D<float> detailShapeNoiseTexture;  // Null texture.
	StructuredBuffer<float3> atmosphereIrradiance = ResourceDescriptorHeap[bindData.atmosphereIrradianceBuffer];
	Texture2D<float3> weatherTexture = ResourceDescriptorHeap[bindData.weatherTexture];
	Texture2D<float> geometryDepthTexture = ResourceDescriptorHeap[bindData.geometryDepthTexture];
	Texture2D<float> blueNoiseTexture = ResourceDescriptorHeap[bindData.blueNoiseTexture];
	
	const float planetRadius = 6360.0;  // #TODO: Get from atmosphere data.
	
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
		return 0;
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
	
	// TODO: early out if we hit a cloud in screenspace too!

	if (marchEnd <= marchStart)
	{
		return 0;
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
	float accumulatedTransmittance = 1.f;  // Used for early out once the ray is fully shadowed
#ifdef CLOUDS_DEBUG_MARCHCOUNT
	int totalSteps = 0;
#endif

	while (dist < marchEnd)
	{
		float3 position = origin + rayDirection * dist;
		
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
		
		// March towards the sun.
		float3 scatteredLuminance;
		float transmittance;
		float depth;  // Kilometers.
#ifdef CLOUDS_DEBUG_MARCHCOUNT
		int stepCount = RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, atmosphereIrradiance, weatherTexture,
			position, sunDirection, 0.f, localMarchStart, localMarchEnd, sunDirection, bindData.wind, bindData.time,
			scatteredLuminance, transmittance, depth);
		totalSteps += stepCount;
#else
		RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, atmosphereIrradiance, weatherTexture,
			position, sunDirection, 0.f, localMarchStart, localMarchEnd, sunDirection, bindData.wind, bindData.time,
			scatteredLuminance, transmittance, depth);
#endif
		
		// Experimenting with weighted contribution to reduce shadows far away.
		float weight = 15.f - 0.12 * pow(dist, 1.5f);
		weight = saturate(weight);

		if (transmittance < 1.f)
		{
			totalShadow += stepSize * (1.f - transmittance) * weight;
			accumulatedTransmittance *= transmittance;
		}

		// Early out if the sample is fully shadowed OR the effective contribution is 0.
		if (accumulatedTransmittance < 0.01f || weight <= 0.f)
			break;

		dist += stepSize;
	}
	
#ifdef CLOUDS_DEBUG_MARCHCOUNT
	return float(totalSteps);
#else
	return totalShadow;
#endif
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];
	
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
	
	float shadowLength = RayMarch(camera, uv, jitteredUv, width, height);
	
	outputTexture[dispatchId.xy] = shadowLength;
}