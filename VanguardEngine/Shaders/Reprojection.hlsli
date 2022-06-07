// Copyright (c) 2019-2022 Andrew Depke

#ifndef __REPROJECTION_HLSLI__
#define __REPROJECTION_HLSLI__

#include "Camera.hlsli"

float2 ReprojectUv(Camera camera, float2 inputUv, float depth)
{
	matrix viewProjection = mul(camera.view, camera.projection);
	matrix inverseViewProjection = mul(camera.lastFrameInverseProjection, camera.lastFrameInverseView);

	// Convert to world space of the previous frame.
	float4 clipSpace = UvToClipSpace(inputUv);
	float4 worldSpace = mul(clipSpace, inverseViewProjection);
	worldSpace /= worldSpace.w;

	// Scale by the depth.
	float3 ray = normalize(worldSpace.xyz - camera.position.xyz);
	worldSpace = float4(camera.position + ray * depth, 1.f);
	
	// Convert back to clip space of the current frame.
	float4 reprojected = mul(worldSpace, viewProjection);
	reprojected /= reprojected.w;

	float2 delta = inputUv - ClipSpaceToUv(reprojected);
	return inputUv + delta;
}

#endif  // __REPROJECTION_HLSLI__