// Copyright (c) 2019-2021 Andrew Depke

#ifndef __CAMERA_HLSLI__
#define __CAMERA_HLSLI__

#pragma pack_matrix(row_major)

struct CameraData
{
	matrix viewMatrix;
	matrix projectionMatrix;
	float3 position;
	float padding;
};

// #TODO: Near and far plane, FOV.
struct Camera
{
    float3 position;  // World space.
    float padding;
};

float LinearizeDepth(float hyperbolicDepth)
{
	// #TODO: Pass the plane values to the shader dynamically.
	float nearPlane = 5000.f;
	float farPlane = 1.f;

	return (2.f * farPlane) / (nearPlane + farPlane - (1.f - hyperbolicDepth) * (nearPlane - farPlane));
}

#endif  // __CAMERA_HLSLI__