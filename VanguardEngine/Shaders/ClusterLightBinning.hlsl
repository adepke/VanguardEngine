// Copyright (c) 2019-2021 Andrew Depke

#include "Light.hlsli"
#include "Geometry.hlsli"

#define RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(b0, num32BitConstants = 1)," \
    "SRV(t0)," \
    "SRV(t1)," \
    "SRV(t2)," \
    "UAV(u0)," \
    "UAV(u1)," \
    "UAV(u2)," \
    "UAV(u3)," \
    "CBV(b1)"

struct LightCount
{
    uint count;
};

// #TEMP
RWStructuredBuffer<uint> testing : register(u3);
ConstantBuffer<Camera> camera : register(b1);

StructuredBuffer<uint> denseClusterList : register(t0);
StructuredBuffer<AABB> clusterBounds : register(t1);
ConstantBuffer<LightCount> lightCount : register(b0);
StructuredBuffer<Light> lights : register(t2);
RWStructuredBuffer<uint> lightCounter : register(u0);
RWStructuredBuffer<uint> lightList : register(u1);
RWStructuredBuffer<uint2> clusterLightInfo : register(u2);

static const uint threadGroupSize = 64;

groupshared AABB froxelBounds;
groupshared uint localLightCount;
groupshared uint localLightList[MAX_LIGHTS_PER_FROXEL];
groupshared uint globalLightListOffset;

bool LightInFroxel(Light light, AABB aabb)
{
    // #TODO: Precompute view space position of each light in a separate pass to prevent redundant work.
    float4 viewSpace = mul(float4(light.position, 1.f), camera.view);
    
    return SphereAABBIntersection(viewSpace.xyz, ComputeLightRadius(light), aabb);
}

// Bins all lights into froxels. One thread group per froxel.
[RootSignature(RS)]
[numthreads(threadGroupSize, 1, 1)]
void ComputeLightBinsMain(uint3 dispatchId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // testing
    if (groupId.x >= testing[0])
    {
        return;
    }
    
    // Only one thread in the group needs to initialize group variables and find the froxel bounds.
    if (groupIndex == 0)
    {
        localLightCount = 0;
        
        // The dense cluster list contains cluster indices of active froxels. Assuming 1-dimensional dispatch.
        froxelBounds = clusterBounds[denseClusterList[groupId.x]];
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Interleaved iteration between all threads in the group. Divides the work evenly.
    for (uint i = groupIndex; i < lightCount.count; i += threadGroupSize)
    {
        if (LightInFroxel(lights[i], froxelBounds))
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