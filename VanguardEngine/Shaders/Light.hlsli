// Copyright (c) 2019-2021 Andrew Depke

#ifndef __LIGHT_HLSLI__
#define __LIGHT_HLSLI__

#include "BRDF.hlsli"
#include "Material.hlsli"
#include "Camera.hlsli"

static const uint LightTypePoint = 0;
static const uint LightTypeSpotlight = 1;
static const uint LightTypeDirectional = 2;

struct Light
{
	float3 position;
	uint type;
	// Boundary
	float3 color;  // Combined diffuse and specular.
	float luminance;
};

float ComputeLightRadius(Light light)
{
	// #TEMP
	return 15.f;
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
	
	float3 lightDirection = normalize(light.position - surfacePosition);
	float3 halfwayDirection = normalize(viewDirection + lightDirection);

	float distance = length(light.position - surfacePosition);
	float attenuation = ComputeLightAttenuation(light, distance);
	float3 radiance = light.color * attenuation;

	output.diffuse.rgb = BRDF(surfaceNormal, viewDirection, halfwayDirection, lightDirection, material.baseColor.rgb, material.metalness, material.roughness, radiance);
	output.diffuse.a = 1.0;
	
	return output;
}

#endif  // __LIGHT_HLSLI__