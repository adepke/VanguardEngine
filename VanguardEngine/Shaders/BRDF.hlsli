// Copyright (c) 2019-2021 Andrew Depke

#ifndef __BRDF_HLSLI__
#define __BRDF_HLSLI__

#include "Constants.hlsli"

// Implementation of Cook-Torrance BRDF.

// Diffuse BRDF component.
float3 LambertianDiffuse(float3 baseColor)
{
	return baseColor / pi;
}

// Normal distribution function.
float3 TrowbridgeReitzGGX(float3 normal, float3 halfway, float roughness)
{
	const float roughnessSquared = roughness * roughness;
	const float dotProduct = saturate(dot(normal, halfway));
	const float dotProductSquared = dotProduct * dotProduct;
	const float denominator = dotProductSquared * (roughnessSquared - 1.0) + 1.0;

	return roughnessSquared / (pi * denominator * denominator);
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
	switch (type)
	{
		case LightingType::Direct:
			return ((roughness + 1.0) * (roughness + 1.0)) / 8.0;
		case LightingType::IBL:
			return (roughness * roughness) / 2.0;
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

	return DFG / max(4.0 * saturate(dot(view, normal)) * saturate(dot(light, normal)), 0.001);
}

float3 BRDF(float3 normal, float3 view, float3 halfway, float3 light, float3 baseColor, float metalness, float roughness, float3 radiance)
{
	const float3 fNaught = lerp(float3(0.04, 0.04, 0.04), baseColor, metalness);

	const float3 specularFactor = FresnelSchlick(saturate(dot(view, halfway)), fNaught);
	const float3 diffuseFactor = (float3(1.0, 1.0, 1.0) - specularFactor) * (1.0 - metalness);

	const float3 weightedRadiance = radiance * saturate(dot(normal, light));

	return (diffuseFactor * LambertianDiffuse(baseColor) + CookTorranceSpecular(normal, view, halfway, light, roughness, specularFactor)) * weightedRadiance;
}

#endif  // __BRDF_HLSLI__