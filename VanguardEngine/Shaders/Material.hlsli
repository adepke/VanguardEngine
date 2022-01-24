// Copyright (c) 2019-2022 Andrew Depke

#ifndef __MATERIAL_HLSLI__
#define __MATERIAL_HLSLI__

struct MaterialData
{
	uint baseColor;
	uint metallicRoughness;
	uint normal;
	uint occlusion;
	// Boundary
	uint emissive;
	float3 emissiveFactor;
	// Boundary
	float4 baseColorFactor;
	// Boundary
	float metallicFactor;
	float roughnessFactor;
	float2 padding;
};

struct Material
{
	float4 baseColor;
	// Boundary
	float metalness;
	float3 normal;
	// Boundary
	float roughness;
	float3 emissive;
	// Boundary
	float occlusion;
	float3 padding;
};

#endif  // __MATERIAL_HLSLI__