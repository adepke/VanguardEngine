// Copyright (c) 2019-2021 Andrew Depke

#ifndef __CLUSTERS_HLSLI__
#define __CLUSTERS_HLSLI__

#include "Camera.hlsli"

uint ClusterId2Index(uint3 dimensions, uint3 id)
{
	return id.x + (dimensions.x * (id.y + dimensions.y * id.z));
}

uint3 ClusterIndex2Id(uint3 dimensions, uint index)
{
	return uint3(index % dimensions.x, index % (dimensions.x * dimensions.y) / dimensions.x, index / (dimensions.x * dimensions.y));
}

uint3 DrawToClusterId(uint froxelSize, float logY, Camera camera, float2 positionSS, float depthViewSpace)
{
	return uint3(positionSS.x / froxelSize, positionSS.y / froxelSize, log(-depthViewSpace / camera.nearPlane) * logY);
}

#endif  // __CLUSTERS_HLSLI__