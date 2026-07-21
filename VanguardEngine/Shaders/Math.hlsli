// Copyright (c) 2019-2022 Andrew Depke

#ifndef __MATH_HLSLI__
#define __MATH_HLSLI__

float RemapRange(float value, float inMin, float inMax, float outMin, float outMax)
{
	return outMin + (((value - inMin) / (inMax - inMin)) * (outMax - outMin));
}

float3x3 ComputeOrthonormalBasis(float3 n)
{
	// Duff et al. 2017, "Building an Orthonormal Basis, Revisited".
	// https://jcgt.org/published/0006/01/01/
	const float s = n.z >= 0.f ? 1.f : -1.f;
	const float a = -1.f / (s + n.z);
	const float b = n.x * n.y * a;

	const float3 tangent = float3(1.f + s * n.x * n.x * a, s * b, -s * n.x);
	const float3 bitangent = float3(b, s + n.y * n.y * a, -n.y);

	return float3x3(tangent, bitangent, n);
}

#endif  // __MATH_HLSLI__