// Copyright (c) 2019-2022 Andrew Depke

#ifndef __MATERIAL_HLSLI__
#define __MATERIAL_HLSLI__

static const uint materialFlagAlphaMask = 1 << 0;
static const uint materialFlagAlphaBlend = 1 << 1;
static const uint materialFlagDoubleSided = 1 << 2;

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
	float alphaCutoff;
    uint flags;
};

// Returns true when the fragment should be discarded.
bool MaterialAlphaTest(MaterialData material, float alpha)
{
	// #TODO: this is wrong for blending, need to properly support transparents.
	if (material.flags & (materialFlagAlphaMask | materialFlagAlphaBlend))
	{
		return alpha < material.alphaCutoff;
	}

	return false;
}

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