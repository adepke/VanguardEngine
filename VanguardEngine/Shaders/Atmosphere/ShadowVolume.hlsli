// Copyright (c) 2019-2022 Andrew Depke

#ifndef __SHADOWVOLUME_HLSLI__
#define __SHADOWVOLUME_HLSLI__

#include "Camera.hlsli"
#include "Geometry.hlsli"
#include "Constants.hlsli"

// Returns the distance traversed by the camera ray, as well as the subset of that ray in shadow, in kilometers.
float2 RayMarchCloudShadowVolume(Camera camera, float2 uv, Camera sunCamera, Texture2D<float> cloudsShadowMap)
{
    // Crude approximation. Replace this with sampling a depth map, at the same UV coords that we sample the cloudsShadowMap (during raymarch)
    float topBoundary = cloudLayerBottom + 0.5f * (cloudLayerTop - cloudLayerBottom);
    
    const float3 rayDirection = ComputeRayDirection(camera, uv);
    const float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	
    float marchStart;
    float marchEnd;
	
    const float planetRadius = 6360.0;  // #TODO: Get from atmosphere data.
    
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
	
    const int steps = 80;
    
    // Dynamically increase step size at quadradic rate, as detail is important close to the camera
    // but coverage is important far away.
    const float initialStepSize = 0.03;
	
    matrix sunViewProjection = mul(sunCamera.view, sunCamera.projection);
	
    float stepSize = initialStepSize;
    float totalDistance = 0.f;
    float totalShadow = 0.f;
	
    for (int i = 0; i < steps; ++i)
    {
        // Model the step size as a parabola for stable growth without runaway
        stepSize = 0.0002 * pow(i, 2) + 0.001 * i + initialStepSize;
        
        float3 position = cameraPosition + (rayDirection * (totalDistance + stepSize));
		
        float dist = totalDistance + stepSize;
        if (dist > (marchEnd - marchStart))
            break;  // Left the cloud layer.
		
		// Computed the ray position, now sample the shadow volume.
        float4 positionSunSpace = mul(float4(position, 1.0), sunViewProjection);
        float3 projectedCoords = positionSunSpace.xyz / positionSunSpace.w;
        projectedCoords = projectedCoords * 0.5 + 0.5;
		
        float shadowSample = cloudsShadowMap.Sample(bilinearClamp, projectedCoords.xy);
        // #TODO: Scale the shadow contribution by the transmittance at this point, otherwise the clouds are treated
        // as entirely opaque solid objects.
        totalShadow += (shadowSample > 0 && shadowSample < 10000) ? stepSize : 0;
        totalDistance += stepSize;
    }
	
    return float2(totalDistance, totalShadow);
}

#endif  // __SHADOWVOLUME_HLSLI__