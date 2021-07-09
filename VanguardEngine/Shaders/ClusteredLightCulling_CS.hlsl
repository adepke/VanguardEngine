// Copyright (c) 2019-2021 Andrew Depke

#include "ClusteredLightCulling_RS.hlsli"
#include "Geometry.hlsli"
#include "Camera.hlsli"
#include "Clusters.hlsli"

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
RWStructuredBuffer<ViewFrustum> clusterFrustums : register(u0);
RWStructuredBuffer<AABB> clusterAABBs : register(u1);

// Computes the frustum for each cluster tile.
[RootSignature(RS)]
[numthreads(8, 8, 1)]
void ComputeClusterFrustumsMain(uint3 dispatchId : SV_DispatchThreadID)
{
    float3 frustumVertices[4];
    frustumVertices[0] = ClipToViewSpace(camera, UvToClipSpace(((dispatchId.xy + float2(0.f, 0.f)) * FROXEL_SIZE) / clusterData.resolution)).xyz;  // Top left.
    frustumVertices[1] = ClipToViewSpace(camera, UvToClipSpace(((dispatchId.xy + float2(1.f, 0.f)) * FROXEL_SIZE) / clusterData.resolution)).xyz;  // Top right.
    frustumVertices[2] = ClipToViewSpace(camera, UvToClipSpace(((dispatchId.xy + float2(0.f, 1.f)) * FROXEL_SIZE) / clusterData.resolution)).xyz;  // Bottom left.
    frustumVertices[3] = ClipToViewSpace(camera, UvToClipSpace(((dispatchId.xy + float2(1.f, 1.f)) * FROXEL_SIZE) / clusterData.resolution)).xyz;  // Bottom right.
    
    float3 origin = float3(0.f, 0.f, 0.f);
    
    ViewFrustum frustum;
    frustum.planes[0] = ComputePlane(origin, frustumVertices[2], frustumVertices[0]);  // Left.
    frustum.planes[1] = ComputePlane(origin, frustumVertices[1], frustumVertices[3]);  // Right.
    frustum.planes[2] = ComputePlane(origin, frustumVertices[0], frustumVertices[1]);  // Top.
    frustum.planes[3] = ComputePlane(origin, frustumVertices[3], frustumVertices[2]);  // Bottom.
    
    uint index = DispatchToClusterIndex(clusterData.gridDimensions, dispatchId);
    if (index < clusterData.gridDimensions.x * clusterData.gridDimensions.y)
    {
        clusterFrustums[index] = frustum;
    }
}

// Computes the AABB for each froxel.
[RootSignature(RS)]
[numthreads(64, 1, 1)]
void ComputeClusterBoundsMain(uint3 dispatchId : SV_DispatchThreadID)
{
    uint3 clusterIndex = uint3(dispatchId.x % clusterData.gridDimensions.x, dispatchId.x % (clusterData.gridDimensions.x * clusterData.gridDimensions.y) / clusterData.gridDimensions.x, dispatchId.x / (clusterData.gridDimensions.x * clusterData.gridDimensions.y));
    
    float3 topLeft = ClipToViewSpace(camera, UvToClipSpace((clusterIndex.xy * FROXEL_SIZE) / (float2)clusterData.resolution)).xyz;
    float3 bottomRight = ClipToViewSpace(camera, UvToClipSpace(((clusterIndex.xy + 1) * FROXEL_SIZE) / (float2)clusterData.resolution)).xyz;
    
    Plane near = { 0.f, 0.f, 1.f, -camera.nearPlane * pow(abs(clusterData.nearK), clusterIndex.z) };
    Plane far = { 0.f, 0.f, 1.f, -camera.nearPlane * pow(abs(clusterData.nearK), clusterIndex.z + 1) };
    
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
        clusterAABBs[dispatchId.x] = box;
    }
}