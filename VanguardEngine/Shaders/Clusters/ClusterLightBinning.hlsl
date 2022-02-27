// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Light.hlsli"
#include "Geometry.hlsli"

struct BindData
{
	uint cameraBuffer;
	uint cameraIndex;
	uint denseClusterListBuffer;
	uint clusterBoundsBuffer;
	uint lightsBuffer;
	uint lightCount;
	uint lightCounterBuffer;
	uint lightListBuffer;
	uint lightInfoBuffer;
};

ConstantBuffer<BindData> bindData : register(b0);

static const uint threadGroupSize = 64;

groupshared AABB froxelBounds;
groupshared uint localLightCount;
groupshared uint localLightList[MAX_LIGHTS_PER_FROXEL];
groupshared uint globalLightListOffset;

bool LightInFroxel(Camera camera, Light light, AABB aabb)
{
	switch (light.type)
	{
		case LightType::Point:
		{
			// #TODO: Precompute view space position of each light in a separate pass to prevent redundant work.
			float4 viewSpace = mul(float4(light.position, 1.f), camera.view);
			return SphereAABBIntersection(viewSpace.xyz, ComputeLightRadius(light), aabb);
		}
		case LightType::Directional:
			return true;
	}
	
	return false;
}

// Bins all lights into froxels. One thread group per froxel.
[RootSignature(RS)]
[numthreads(threadGroupSize, 1, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	StructuredBuffer<uint> denseClusterList = ResourceDescriptorHeap[bindData.denseClusterListBuffer];
	StructuredBuffer<AABB> clusterBounds = ResourceDescriptorHeap[bindData.clusterBoundsBuffer];
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[bindData.lightsBuffer];
	RWStructuredBuffer<uint> lightCounter = ResourceDescriptorHeap[bindData.lightCounterBuffer];
	RWStructuredBuffer<uint> lightList = ResourceDescriptorHeap[bindData.lightListBuffer];
	RWStructuredBuffer<uint2> clusterLightInfo = ResourceDescriptorHeap[bindData.lightInfoBuffer];

	// Only one thread in the group needs to initialize group variables and find the froxel bounds.
	if (groupIndex == 0)
	{
		localLightCount = 0;
		
		// The dense cluster list contains cluster indices of active froxels. Assuming 1-dimensional dispatch.
		froxelBounds = clusterBounds[denseClusterList[groupId.x]];
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// Interleaved iteration between all threads in the group. Divides the work evenly.
	for (uint i = groupIndex; i < bindData.lightCount; i += threadGroupSize)
	{
		if (LightInFroxel(camera, lights[i], froxelBounds))
		{
			uint index;
			InterlockedAdd(localLightCount, 1, index);
			if (index < MAX_LIGHTS_PER_FROXEL)
			{
				localLightList[index] = i;
			}
		}
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// Allocate a chunk from the global light list to copy our local list into and set the offset/count into the global list for the froxel.
	if (groupIndex == 0)
	{
		localLightCount = min(localLightCount, MAX_LIGHTS_PER_FROXEL);  // Prevent overflowing light bins.
		InterlockedAdd(lightCounter[0], localLightCount, globalLightListOffset);
		clusterLightInfo[denseClusterList[groupId.x]] = uint2(globalLightListOffset, localLightCount);
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// Merge the local light list with our partition in the global list.
	for (uint i = groupIndex; i < localLightCount; i += threadGroupSize)
	{
		lightList[globalLightListOffset + i] = localLightList[i];
	}
}