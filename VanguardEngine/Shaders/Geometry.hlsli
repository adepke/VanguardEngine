// Copyright (c) 2019-2021 Andrew Depke

#ifndef __GEOMETRY_HLSLI__
#define __GEOMETRY_HLSLI__

struct Plane
{
    float3 normal;
    float distance;
};

struct ViewFrustum
{
    Plane planes[4];  // Left, right, top, bottom.
};

struct AABB
{
    float4 min;
    float4 max;
};

Plane ComputePlane(float3 a, float3 b, float3 c)
{
    Plane result;
    result.normal = normalize(cross(b - a, c - a));
    result.distance = dot(result.normal, a);
    
    return result;
}

bool LinePlaneIntersection(float3 a, float3 b, Plane plane, out float3 q)
{
    float3 vec = b - a;
    float t = (plane.distance - dot(plane.normal, a)) / dot(plane.normal, vec);
    
    if (t >= 0.f && t <= 1.f)
    {
        q = a + t * vec;
        return true;
    }
    
    q = 0.f;
    return false;
}

#endif  // __GEOMETRY_HLSLI__