// Copyright (c) 2019-2021 Andrew Depke

#ifndef __CAMERA_HLSLI__
#define __CAMERA_HLSLI__

#pragma pack_matrix(row_major)

struct Camera
{
    float4 position;  // World space.
    // Boundary
    matrix view;
    matrix projection;
    matrix inverseView;
    matrix inverseProjection;
    // Boundary
    float nearPlane;
    float farPlane;
    float fieldOfView;  // Horizontal, radians.
    float aspectRatio;
};

float LinearizeDepth(Camera camera, float hyperbolicDepth)
{
	// Reversed planes for the inverse depth buffer.
	float nearPlane = camera.farPlane;
	float farPlane = camera.nearPlane;

	return (2.f * farPlane) / (nearPlane + farPlane - (1.f - hyperbolicDepth) * (nearPlane - farPlane));
}

float3 ComputeRayDirection(Camera camera, float2 uv)
{
    uv = float2(uv.x, 1.f - uv.y);  // Reverse the y axis.
    uv = uv * 2.f - 1.f;  // Remap to [-1, 1].
    
    float4 positionClipSpace = float4(uv, 0.f, 1.f);  // Clip space Z of 0 due to the inverse depth buffer.
    float4 positionViewSpace = float4(mul(camera.inverseProjection, positionClipSpace).xyz, 0.f);
    float4 positionWorldSpace = mul(camera.inverseView, positionViewSpace);
    
    return normalize(positionWorldSpace.xyz);
}

#endif  // __CAMERA_HLSLI__