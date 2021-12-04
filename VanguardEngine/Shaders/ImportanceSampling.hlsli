// Copyright (c) 2019-2021 Andrew Depke

#ifndef __IMPORTANCE_SAMPLING_HLSLI__
#define __IMPORTANCE_SAMPLING_HLSLI__

#include "Constants.hlsli"

float VanDerCorputSequence(uint bits)
{
	bits = (bits << 16) | (bits >> 16);
	bits = ((bits & 0x55555555) << 1u) | ((bits & 0xAAAAAAAA) >> 1u);
	bits = ((bits & 0x33333333) << 2u) | ((bits & 0xCCCCCCCC) >> 2u);
	bits = ((bits & 0x0F0F0F0F) << 4u) | ((bits & 0xF0F0F0F0) >> 4u);
	bits = ((bits & 0x00FF00FF) << 8u) | ((bits & 0xFF00FF00) >> 8u);
	
	return (float)bits * 2.3283064365386963e-10;
}

// Low-discrepancy set.
float2 HammersleySet(uint i, uint n)
{
	return float2((float)i / (float)n, VanDerCorputSequence(i));
}

// Sequence must be from a low-discrepancy set.
float3 ImportanceSampledGGX(float2 sequence, float3 normal, float roughness)
{
	float roughness2 = roughness * roughness;
	float theta = 2.f * pi * sequence.x;
	float cosPhi = sqrt((1.f - sequence.y) / (1.f + (roughness2 * roughness2 - 1.f) * sequence.y));
	float sinPhi = sqrt(1.f - cosPhi * cosPhi);
	
	float3 tangentSpace = float3(sinPhi * cos(theta), sinPhi * sin(theta), cosPhi);
	float3 up = abs(normal.z) < 0.999 ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	float3 tangent = normalize(cross(up, normal));
	float3 bitangent = cross(normal, tangent);
	
	return normalize(tangentSpace.x * tangent + tangentSpace.y * bitangent + tangentSpace.z * normal);
}

#endif  // __IMPORTANCE_SAMPLING_HLSLI__