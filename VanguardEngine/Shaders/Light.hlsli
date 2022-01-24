// Copyright (c) 2019-2022 Andrew Depke

#ifndef __LIGHT_HLSLI__
#define __LIGHT_HLSLI__

#include "BRDF.hlsli"
#include "Material.hlsli"
#include "Camera.hlsli"

enum class LightType
{
	Point,
	Directional
};

struct Light
{
	float3 position;
	LightType type;
	// Boundary
	float3 color;  // Combined diffuse and specular.
	float luminance;  // Nits.
	float3 direction;
	float padding;
};

float ComputeLightRadius(Light light)
{
	// #TEMP
	return 150.f;
}

float ComputeLightAttenuation(Light light, float distance)
{
	// Physically correct attenuation for light in a vaccum (does not play well with clustered light culling).
	//return 1.f / (distance * distance);
	
	// Note that this is not physically correct, it's used for improved behavior with clustered light culling.
	float range = ComputeLightRadius(light);
	return 1.f - smoothstep(range * 0.75f, range, distance);
}

struct LightSample
{
	float4 diffuse;
};

LightSample SampleLight(Light light, Material material, Camera camera, float3 viewDirection, float3 surfacePosition, float3 surfaceNormal)
{
	LightSample output;
	
	float3 lightDirection;
	float3 radiance = light.color;
	
	switch (light.type)
	{
		case LightType::Point:
		{
			lightDirection  = normalize(light.position - surfacePosition);
			
			float distance = length(light.position - surfacePosition);
			float attenuation = ComputeLightAttenuation(light, distance);
			radiance *= attenuation;
			break;
		}
		case LightType::Directional:
		{
			lightDirection = normalize(-light.direction);
			// #TODO: Directional lights need to be affected the atmosphere's transmittance. Use GetSkyRadiance.
			break;
		}
	}
	
	float3 halfwayDirection = normalize(viewDirection + lightDirection);

	output.diffuse.rgb = BRDF(surfaceNormal, viewDirection, halfwayDirection, lightDirection, material.baseColor.rgb, material.metalness, material.roughness, radiance);
	output.diffuse.a = 1.0;
	
	return output;
}

#endif  // __LIGHT_HLSLI__