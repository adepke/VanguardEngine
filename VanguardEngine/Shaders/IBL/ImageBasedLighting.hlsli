// Copyright (c) 2019-2022 Andrew Depke

#ifndef __IMAGEBASEDLIGHTING_HLSLI__
#define __IMAGEBASEDLIGHTING_HLSLI__

#include "Material.hlsli"
#include "BRDF.hlsli"

// Performs diffuse and specular IBL.
float3 ComputeIBL(float3 normalDirection, float3 viewDirection, Material material, uint prefilterLevels, TextureCube irradianceLut, TextureCube prefilterLut, Texture2D brdfLut, SamplerState lutSampler)
{
	float3 reflectionDirection = reflect(-viewDirection, normalDirection);
	// Note that prefilterLevels is most likely smaller than the mip levels of the prefilter map. This reduces glowing rim artifacts on
	// highly rough metals due to losing too much data in the prefilter map.
	float3 prefilterSample = prefilterLut.SampleLevel(lutSampler, reflectionDirection, material.roughness * (prefilterLevels - 1.f)).rgb;
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