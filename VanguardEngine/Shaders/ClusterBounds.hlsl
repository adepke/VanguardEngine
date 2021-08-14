// Copyright (c) 2019-2021 Andrew Depke

#include "Geometry.hlsli"
#include "Camera.hlsli"
#include "Clusters.hlsli"

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"RootConstants(b0, num32BitConstants = 8)," \
	"CBV(b1)," \
	"UAV(u0)"

struct ClusterData
{
    int3 gridDimensions;
    float nearK;
    // Boundary
    int2 resolution;
    float2 padding;
};

ConstantBuffer<ClusterData> clusterData : register(b0);
ConstantBuffer<Camera> camera : register(b1);
RWStructuredBuffer<AABB> clusterBounds : register(u0);

// Computes the AABB for each froxel.
[RootSignature(RS)]
[numthreads(64, 1, 1)]
void ComputeClusterBoundsMain(uint3 dispatchId : SV_DispatchThreadID)
{
    uint3 clusterId = ClusterIndex2Id(clusterData.gridDimensions, dispatchId.x);
    
    float3 topLeft = ClipToViewSpace(camera, UvToClipSpace((clusterId.xy * FROXEL_SIZE) / (float2)clusterData.resolution)).xyz;
    float3 bottomRight = ClipToViewSpace(camera, UvToClipSpace(((clusterId.xy + 1) * FROXEL_SIZE) / (float2)clusterData.resolution)).xyz;
    
    Plane near = { 0.f, 0.f, 1.f, -camera.nearPlane * pow(abs(clusterData.nearK), clusterId.z) };
    Plane far = { 0.f, 0.f, 1.f, -camera.nearPlane * pow(abs(clusterData.nearK), clusterId.z + 1) };
    
    float3 origin = float3(0.f, 0.f, 0.f);
    
    float3 minNear, maxNear;
    float3 minFar, maxFar;
    LinePlaneIntersection(origin, topLeft, near, minNear);
    LinePlaneIntersection(origin, bottomRight, near, maxNear);
    LinePlaneIntersection(origin, topLeft, far, minFar);
    LinePlaneIntersection(origin, bottomRight, far, maxFar);
    
    float3 minBound = min(minNear, min(maxNear, min(minFar, maxFar)));
    float3 maxBound = max(minNear, max(maxNear, max(minFar, maxFar)));
    
    AABB box = { float4(minBound, 1.f), float4(maxBound, 1.f) };
    if (dispatchId.x < clusterData.gridDimensions.x * clusterData.gridDimensions.y * clusterData.gridDimensions.z)
    {
        clusterBounds[dispatchId.x] = box;
    }
}