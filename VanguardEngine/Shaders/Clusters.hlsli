// Copyright (c) 2019-2021 Andrew Depke

#ifndef __CLUSTERS_HLSLI__
#define __CLUSTERS_HLSLI__

#include "Camera.hlsli"

// Assumes that one dispatch thread is assigned to each cluster.
uint DispatchToClusterIndex(uint3 dimensions, uint3 dispatchId)
{
    return dispatchId.x + (dimensions.x * (dispatchId.y + dimensions.y * dispatchId.z));
}

uint3 DrawToClusterIndex3D(uint froxelSize, float logY, Camera camera, float2 positionSS, float z)
{
    // Far plane due to inverse depth buffer.
    return uint3(positionSS.x / froxelSize, positionSS.y / froxelSize, log(-z / camera.farPlane) * logY);
}

#endif  // __CLUSTERS_HLSLI__