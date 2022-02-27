// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Geometry.hlsli"
#include "Camera.hlsli"
#include "Clusters/Clusters.hlsli"

struct BindData
{
	int3 gridDimensions;
	float nearK;
	// Boundary
	int2 resolution;
	uint cameraBuffer;
	uint cameraIndex;
	uint boundsBuffer;
};

ConstantBuffer<BindData> bindData : register(b0);

// Computes the AABB for each froxel.
[RootSignature(RS)]
[numthreads(64, 1, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	RWStructuredBuffer<AABB> clusterBounds = ResourceDescriptorHeap[bindData.boundsBuffer];

	uint3 clusterId = ClusterIndex2Id(bindData.gridDimensions, dispatchId.x);
	
	float3 topLeft = ClipToViewSpace(camera, UvToClipSpace((clusterId.xy * FROXEL_SIZE) / (float2)bindData.resolution)).xyz;
	float3 bottomRight = ClipToViewSpace(camera, UvToClipSpace(((clusterId.xy + 1) * FROXEL_SIZE) / (float2)bindData.resolution)).xyz;
	
	Plane near = { 0.f, 0.f, 1.f, -camera.nearPlane * pow(abs(bindData.nearK), clusterId.z) };
	Plane far = { 0.f, 0.f, 1.f, -camera.nearPlane * pow(abs(bindData.nearK), clusterId.z + 1) };
	
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
	if (dispatchId.x < bindData.gridDimensions.x * bindData.gridDimensions.y * bindData.gridDimensions.z)
	{
		clusterBounds[dispatchId.x] = box;
	}
}