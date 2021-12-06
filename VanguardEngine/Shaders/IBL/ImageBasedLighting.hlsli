// Copyright (c) 2019-2021 Andrew Depke

#ifndef __IMAGEBASEDLIGHTING_HLSLI__
#define __IMAGEBASEDLIGHTING_HLSLI__

#include "Material.hlsli"
#include "BRDF.hlsli"

// #TODO: Use BRDF.hlsli

// Performs diffuse and specular IBL.
float3 ComputeIBL(float3 normalDirection, float3 viewDirection, Material material, TextureCube irradianceLut, TextureCube prefilterLut, Texture2D brdfLut, SamplerState lutSampler)
{
	float width, height, prefilterMipCount;
	prefilterLut.GetDimensions(0, width, height, prefilterMipCount);
	
	float3 reflectionDirection = reflect(-viewDirection, normalDirection);
	float3 prefilterSample = prefilterLut.SampleLevel(lutSampler, reflectionDirection, material.roughness * prefilterMipCount).rgb;
	float2 brdf = brdfLut.Sample(lutSampler, float2(saturate(dot(normalDirection, viewDirection)), material.roughness)).xy;
	
	float3 fNaught = lerp(0.04.xxx, material.baseColor.rgb, material.metalness);
	float3 fresnel = FresnelSchlick(saturate(dot(normalDirection, viewDirection)), fNaught, material.roughness);
	float3 specular = prefilterSample * (fresnel * brdf.x + brdf.y);
	
	float3 specularFactor = fresnel;
	float3 diffuseFactor = (1.f - specularFactor) * (1.f - material.metalness);
	float3 irradiance = irradianceLut.Sample(lutSampler, normalDirection).rgb;
	float3 diffuse = irradiance * material.baseColor.rgb;
	
	return (diffuseFactor * diffuse + specular) * material.occlusion;
}

#endif  // __IMAGEBASEDLIGHTING_HLSLI__