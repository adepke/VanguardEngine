// Copyright (c) 2019-2022 Andrew Depke

// Procedural reference grid drawn over the planet surface. Uses the "pristine grid" approach.
// Credit: https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Constants.hlsli"
#include "Geometry.hlsli"

struct BindData
{
	uint cameraBuffer;
	uint cameraIndex;
	float gridHeightMeters;
	float majorCellMeters;
	float minorCellMeters;
	float gridAlpha;
	float fadeStartMeters;
	float fadeEndMeters;
	float3 gridColor;
	float padding;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VSInput
{
	uint vertexID : SV_VertexID;
};

struct PSInput
{
	float4 positionCS : SV_POSITION;
	float2 uv : UV;
};

struct PSOutput
{
	float4 color : SV_Target;
	float depth : SV_Depth;
};

// Width is relative to the cell area.
static const float majorLineWidth = 0.001f;
static const float minorLineWidth = 0.001f;

// Returns the line coverage in [0, 1] for a UV-space grid where each
// integer-spaced gridline has the requested UV-space lineWidth. Length-of-gradient form (instead
// of fwidth's Manhattan |ddx|+|ddy|) gives slightly less aliasing on diagonals.
float PristineGrid(float2 uv, float2 lineWidth)
{
	float4 uvDDXY = float4(ddx(uv), ddy(uv));
	float2 uvDeriv = float2(length(uvDDXY.xz), length(uvDDXY.yw));
	bool2 invertLine = lineWidth > 0.5;
	float2 targetWidth = select(invertLine, 1.0 - lineWidth, lineWidth);
	float2 drawWidth = clamp(targetWidth, uvDeriv, 0.5);
	float2 lineAA = uvDeriv * 1.5;
	float2 gridUV = abs(frac(uv) * 2.0 - 1.0);
	gridUV = select(invertLine, gridUV, 1.0 - gridUV);
	float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
	grid2 *= saturate(targetWidth / drawWidth);  // Brightness compensation when the draw was forced wider than the target.
	grid2 = lerp(grid2, targetWidth, saturate(uvDeriv * 2.0 - 1.0));  // Fade to flat coverage when cells become smaller than 2 pixels.
	grid2 = select(invertLine, 1.0 - grid2, grid2);
	return lerp(grid2.x, 1.0, grid2.y);
}

[RootSignature(RS)]
PSInput VSMain(VSInput input)
{
	PSInput output;
	output.uv = float2((input.vertexID << 1) & 2, input.vertexID & 2);
	output.positionCS = float4((output.uv.x - 0.5) * 2.0, -(output.uv.y - 0.5) * 2.0, 0, 1);
	
	return output;
}

[RootSignature(RS)]
PSOutput PSMain(PSInput input)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	// Reconstruct the world-space ray for this pixel. The view ray is computed using the same
	// trick as Camera.hlsli's ComputeRayDirection: clip space at z=0 (far plane in inverse-Z),
	// transformed back to world without perspective division and treated as a direction.
	float3 rayDirection = ComputeRayDirection(camera, input.uv);
	float3 rayOriginMeters = camera.position.xyz;

	// Wrap the grid around the planet surface, instead of being perfectly flat.
	float3 rayOriginKm = rayOriginMeters / 1000.0;
	float gridSphereRadiusKm = -planetCenter.z + (bindData.gridHeightMeters / 1000.0);

	// Note: do not early discard! Derivates share across the 2x2 pixel quad, so always evaluate the grid first.
	float2 intersections;
	bool hit = RaySphereIntersection(rayOriginKm, rayDirection, planetCenter, gridSphereRadiusKm, intersections);
	float t = all(intersections > 0) ? min(intersections.x, intersections.y) : max(intersections.x, intersections.y);  // Kilometers.
	float3 hitPointMeters = rayOriginMeters + rayDirection * (t * 1000.0);

	// Project the hit onto the planet's tangent plane near the world origin. World XY is a good
	// approximation here: the planet's "up" near origin is +Z, so XY are the surface tangent
	// directions. The grid metric is therefore correct near the camera and only distorts at
	// distances large enough that distance-fade has already killed the grid.
	float2 majorUV = hitPointMeters.xy / bindData.majorCellMeters;
	float2 minorUV = hitPointMeters.xy / bindData.minorCellMeters;

	float majorMask = PristineGrid(majorUV, majorLineWidth.xx);
	float minorMask = PristineGrid(minorUV,minorLineWidth.xx);
	float gridMask = max(minorMask, majorMask);  // Major mask takes priority.

	// Distance-based alpha fade. Without this the grid would become visual noise at the limit
	// of resolvable cells (Pristine Grid already handles per-line aliasing, but the cumulative
	// gray haze still benefits from a full fade-out).
	float hitDistanceMeters = t * 1000.0;
	float distanceFade = 1.0 - smoothstep(bindData.fadeStartMeters, bindData.fadeEndMeters, hitDistanceMeters);

	float alpha = gridMask * distanceFade * bindData.gridAlpha;
	if (!hit || alpha < 0.001)
	{
		discard;
	}

	float4 hitClip = mul(mul(float4(hitPointMeters, 1.0), camera.view), camera.projection);
	
	PSOutput output;
	output.depth = saturate(hitClip.z / hitClip.w);
	output.color = float4(bindData.gridColor, alpha);
	
	return output;
}
