// Copyright (c) 2019-2022 Andrew Depke

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
	matrix lastFrameView;
	matrix lastFrameProjection;
	matrix lastFrameInverseView;
	matrix lastFrameInverseProjection;
	// Boundary
	float nearPlane;
	float farPlane;
	float fieldOfView;  // Horizontal, radians.
	float aspectRatio;
};

float4 UvToClipSpace(float2 uv)
{
	uv = float2(uv.x, 1.f - uv.y);  // Reverse the y axis.
	uv = uv * 2.f - 1.f;  // Remap to [-1, 1].
	
	return float4(uv, 0.f, 1.f);  // Clip space Z of 0 due to the inverse depth buffer.
}

float2 ClipSpaceToUv(float4 clipSpace)
{
	float2 uv = float2(clipSpace.xy + 1.0) / 2.0;
	return float2(uv.x, 1.0 - uv.y);
}

float4 ClipToViewSpace(Camera camera, float4 clipSpace)
{
	float4 viewSpace = mul(clipSpace, camera.inverseProjection);
	
	return viewSpace / viewSpace.w;  // Perspective division.
}

float LinearizeDepth(Camera camera, float hyperbolicDepth)
{
	// Reversed planes for the inverse depth buffer.
	float nearPlane = camera.farPlane;
	float farPlane = camera.nearPlane;

	return (2.f * farPlane) / (nearPlane + farPlane - (1.f - hyperbolicDepth) * (nearPlane - farPlane));
}

float3 ComputeRayDirection(Camera camera, float2 uv)
{
	float4 positionClipSpace = UvToClipSpace(uv);
	float4 positionViewSpace = float4(mul(positionClipSpace, camera.inverseProjection).xyz, 0.f);  // No perspective division.
	float4 positionWorldSpace = mul(positionViewSpace, camera.inverseView);
	
	return normalize(positionWorldSpace.xyz);
}

#endif  // __CAMERA_HLSLI__