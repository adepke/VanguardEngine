// Copyright (c) 2019-2022 Andrew Depke

#ifndef __SHADOWVOLUME_HLSLI__
#define __SHADOWVOLUME_HLSLI__

#include "Camera.hlsli"
#include "Geometry.hlsli"
#include "Constants.hlsli"

// Returns the length of the camera ray in shadow, in kilometers.
float RayMarchCloudShadowVolume(Camera camera, float2 uv, Camera sunCamera, float cloudsDepth, Texture2D<float> cloudsShadowMap)
{
    // Crude approximation. Replace this with sampling a depth map, at the same UV coords that we sample the cloudsShadowMap (during raymarch)
    float topBoundary = cloudLayerBottom + 0.5f * (cloudLayerTop - cloudLayerBottom);
    
    const float3 rayDirection = ComputeRayDirection(camera, uv);
    const float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	
    float marchStart;
    float marchEnd;
	
    const float planetRadius = 6360.0;  // #TODO: Get from atmosphere data.
    float3 planetCenter = float3(0, 0, -(planetRadius + 1));  // Offset since the world origin is 1km off the planet surface.

    float2 topBoundaryIntersect;
    if (RaySphereIntersection(cameraPosition, rayDirection, planetCenter, planetRadius + topBoundary, topBoundaryIntersect))
    {
		// Inside the cloud layer, only advance the ray start if we're outside of the atmosphere.
        marchStart = max(topBoundaryIntersect.x, 0);
        marchEnd = topBoundaryIntersect.y;
    }
    else
    {
		// Outside of the cloud layer.
        return 0.f;
    }

	// Stop short if we hit the planet.
    float2 planetIntersect;
    if (RaySphereIntersection(cameraPosition, rayDirection, planetCenter, planetRadius, planetIntersect))
    {
        marchEnd = min(marchEnd, planetIntersect.x);
    }

    marchStart = max(0, marchStart);
    marchEnd = max(0, marchEnd);
	
    const int steps = 250;
    const float stepSize = 0.1;
	
    matrix sunViewProjection = mul(sunCamera.view, sunCamera.projection);
	
    float totalShadow = 0.f;
	
    for (int i = 0; i < steps; ++i)
    {
        float3 position = cameraPosition + (rayDirection * stepSize * (float) i);
		
        float dist = stepSize * (float)i;
        if (dist > (marchEnd - marchStart))
            break;  // Left the cloud layer.
		
		// Computed the ray position, now sample the shadow volume.
        float4 positionSunSpace = mul(float4(position, 1.0), sunViewProjection);
        float3 projectedCoords = positionSunSpace.xyz / positionSunSpace.w;
        projectedCoords = projectedCoords * 0.5 + 0.5;
		
        float shadowSample = cloudsShadowMap.Sample(downsampleBorder, projectedCoords.xy);
        // #TODO: Scale the shadow contribution by the transmittance at this point, otherwise the clouds are treated
        // as entirely opaque solid objects.
        totalShadow += (shadowSample > 0 && shadowSample < 10000) ? stepSize : 0;
    }
	
    return totalShadow;
}

#endif  // __SHADOWVOLUME_HLSLI__