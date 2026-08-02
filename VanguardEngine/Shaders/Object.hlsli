// Copyright (c) 2019-2022 Andrew Depke

#ifndef __OBJECT_HLSLI__
#define __OBJECT_HLSLI__

#include "VertexAssembly.hlsli"

#pragma pack_matrix(row_major)

struct ObjectData
{
	matrix worldMatrix;
	VertexMetadata vertexMetadata;
	float3 boundingSphereCenter;  // Local space.
	float boundingSphereRadius;  // World space.
	uint materialIndex;
	float3 padding;
};

#endif  // __OBJECT_HLSLI__