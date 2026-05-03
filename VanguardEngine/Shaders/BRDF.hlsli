// Copyright (c) 2019-2022 Andrew Depke

#ifndef __BRDF_HLSLI__
#define __BRDF_HLSLI__

#include "Constants.hlsli"

// Renormalized Disney/Burley diffuse from Frostbite. Energy-conserving.
// See: https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2014-pbs-frostbite-slides.pdf
float3 DisneyDiffuse(float3 baseColor, float3 normal, float3 view, float3 light, float3 halfway, float roughness)
{
	const float perceptualRoughness = sqrt(saturate(roughness));
	const float NdotL = saturate(dot(normal, light));
	const float NdotV = saturate(dot(normal, view));
	const float LdotH = saturate(dot(light, halfway));

	const float energyBias = lerp(0.0, 0.5, perceptualRoughness);
	const float energyFactor = lerp(1.0, 1.0 / 1.51, perceptualRoughness);

	const float fd90 = energyBias + 2.0 * LdotH * LdotH * perceptualRoughness;
	const float lightScatter = 1.0 + (fd90 - 1.0) * pow(1.0 - NdotL, 5.0);
	const float viewScatter  = 1.0 + (fd90 - 1.0) * pow(1.0 - NdotV, 5.0);

	return baseColor * (lightScatter * viewScatter * energyFactor / pi);
}

// Normal distribution function.
float TrowbridgeReitzGGX(float3 normal, float3 halfway, float roughness)
{
	const float roughness2 = roughness * roughness;
	const float normalDotHalfway = saturate(dot(normal, halfway));
	const float normalDotHalfway2 = normalDotHalfway * normalDotHalfway;
	const float denominator = normalDotHalfway2 * (roughness2 - 1.0) + 1.0 + 0.001;  // Prevent division by 0.

	return roughness2 / (pi * denominator * denominator);
}

// Fresnel approximation function.
float3 FresnelSchlick(float cosine, float3 fNaught)
{
	return fNaught + (1.0 - fNaught) * pow(1.0 - cosine, 5.0);
}

// Fresnel approximation including a roughness term.
float3 FresnelSchlick(float cosine, float3 fNaught, float roughness)
{
	return fNaught + (max(1.0 - roughness, fNaught) - fNaught) * pow(1.0 - cosine, 5.0);
}

enum class LightingType
{
	Direct,
	IBL
};

float ComputeGeometryConstant(LightingType type, float roughness)
{
	// Note roughness here is the squared perceptual roughness.
	const float alpha = saturate(roughness);
	switch (type)
	{
		case LightingType::Direct:
		{
			const float perceptual = sqrt(alpha);
			return ((perceptual + 1.0) * (perceptual + 1.0)) / 8.0;
		}
		case LightingType::IBL:
			return alpha / 2.0;
		default:
			return 0.f;
	}
}

float SchlickGGX(float dotProduct, float geometryConstant)
{
	return dotProduct / (dotProduct * (1.0 - geometryConstant) + geometryConstant);
}

// Geometry function.
float SmithGeometry(float3 normal, float3 view, float3 light, float geometryConstant)
{
	return SchlickGGX(saturate(dot(normal, view)), geometryConstant) * SchlickGGX(saturate(dot(normal, light)), geometryConstant);
}

// Specular BRDF component.
float3 CookTorranceSpecular(float3 normal, float3 view, float3 halfway, float3 light, float roughness, float3 fresnel)
{
	const float3 DFG = TrowbridgeReitzGGX(normal, halfway, roughness) * fresnel * SmithGeometry(normal, view, light, ComputeGeometryConstant(LightingType::Direct, roughness));

	return DFG / (4.0 * saturate(dot(view, normal)) * saturate(dot(light, normal)) + 0.001);  // Prevent division by 0.
}

float3 BRDF(float3 normal, float3 view, float3 halfway, float3 light, float3 baseColor, float metalness, float roughness, float3 radiance)
{
	const float3 fNaught = lerp(float3(0.04, 0.04, 0.04), baseColor, metalness);

	const float3 specularFresnel = FresnelSchlick(saturate(dot(view, halfway)), fNaught);

	// Disney diffuse already encodes Fresnel-style edge attenuation via FD90 and the
	// retroreflection lobes, so don't apply an outer (1 - F) factor here. The only
	// gating is metalness — metals have no diffuse contribution.
	const float3 diffuse = DisneyDiffuse(baseColor, normal, view, light, halfway, roughness) * (1.0 - metalness);
	const float3 specular = CookTorranceSpecular(normal, view, halfway, light, roughness, specularFresnel);

	const float3 weightedRadiance = radiance * saturate(dot(normal, light));

	return (diffuse + specular) * weightedRadiance;
}

#endif  // __BRDF_HLSLI__