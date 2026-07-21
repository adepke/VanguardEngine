// Copyright (c) 2019-2022 Andrew Depke

#ifndef __OCTAHEDRALNORMALS_HLSLI__
#define __OCTAHEDRALNORMALS_HLSLI__

// Octahedral normal encoding, see: https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
// Encoded components are in [-1, 1], suitable for float or SNORM render targets.

float2 OctahedralWrap(float2 v)
{
	return (1.f - abs(v.yx)) * float2(v.x >= 0.f ? 1.f : -1.f, v.y >= 0.f ? 1.f : -1.f);
}

float2 EncodeNormalOctahedral(float3 n)
{
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	n.xy = n.z >= 0.f ? n.xy : OctahedralWrap(n.xy);
	return n.xy;
}

float3 DecodeNormalOctahedral(float2 f)
{
	float3 n = float3(f.x, f.y, 1.f - abs(f.x) - abs(f.y));
	float t = saturate(-n.z);
	n.xy += float2(n.x >= 0.f ? -t : t, n.y >= 0.f ? -t : t);
	return normalize(n);
}

#endif  // __OCTAHEDRALNORMALS_HLSLI__
