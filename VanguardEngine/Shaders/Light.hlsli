// Copyright (c) 2019-2021 Andrew Depke

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
    float padding;
};

struct LightSample
{
    float4 diffuse;
};

LightSample SampleLight(Light light, Material material, Camera camera, float3 viewDirection, float3 surfacePosition, float3 surfaceNormal)
{
    LightSample output;
	
    float3 lightDirection = normalize(light.position - surfacePosition);
    float3 halfwayDirection = normalize(viewDirection + lightDirection);

    float distance = length(light.position - surfacePosition) * 0.002;
    float attenuation = 1.0 / (distance * distance);
    float3 radiance = light.color * attenuation;

    output.diffuse.rgb = BRDF(surfaceNormal, viewDirection, halfwayDirection, lightDirection, material.baseColor.rgb, material.metalness, material.roughness, radiance);
    output.diffuse.a = 1.0;
	
    return output;
}