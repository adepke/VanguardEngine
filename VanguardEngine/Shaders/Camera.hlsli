// Copyright (c) 2019-2021 Andrew Depke

#ifndef __CAMERA_HLSLI__
#define __CAMERA_HLSLI__

#pragma pack_matrix(row_major)

// #TODO: Near and far plane, FOV.
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

#endif  // __CAMERA_HLSLI__