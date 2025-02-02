// Copyright (c) 2019-2022 Andrew Depke

#ifndef __VISIBILITY_HLSLI__
#define __VISIBILITY_HLSLI__

#include "Camera.hlsli"

// Position must be in atmosphere space.
float CalculateSunVisibility(float3 position, Camera sunCamera, Texture2D<float> cloudsShadowMap)
{
    matrix sunViewProjection = mul(sunCamera.view, sunCamera.projection);
    
    // Computed the ray position, now sample the shadow volume.
    float4 positionSunSpace = mul(float4(position, 1.0), sunViewProjection);
    float3 projectedCoords = positionSunSpace.xyz / positionSunSpace.w;
    projectedCoords = projectedCoords * 0.5 + 0.5;
		
    float shadowSample = cloudsShadowMap.Sample(bilinearClamp, projectedCoords.xy);
    
	// Direct occlusion to the sun, just sample the clouds depth map.
    return (shadowSample > 0 && shadowSample < 10000) ? 0.5f : 1.f;
}

// Position must be in atmosphere space.
float CalculateSkyVisibility(float3 position, float globalWeatherCoverage)
{
	// #TODO: Indirect occlusion to the entire sky, use the cloud coverage to approximate this. Only if position is below clouds.
    return 1.f - globalWeatherCoverage;
}

#endif  // __VISIBILITY_HLSLI__