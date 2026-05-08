// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Geometry.hlsli"

#define SPHERE_GREAT_CIRCLES 3
#ifndef SPHERE_SEGMENTS
#define SPHERE_SEGMENTS 32  // Tessellation density per great circle. Vertices per sphere = 3 * SPHERE_SEGMENTS * 2.
#endif

struct DebugCube
{
	matrix transform;  // World-space transform applied to a unit cube spanning [-0.5, 0.5].
	float4 color;
};

struct DebugSphere
{
	float3 center;
	float radius;
	float4 color;
};

struct BindData
{
	uint cameraBuffer;
	uint cameraIndex;
	uint shapeBuffer;  // Interpretation depends on vertex shader.
	float padding;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VSInput
{
	uint vertexId : SV_VertexID;
	uint instanceId : SV_InstanceID;
};

struct PSInput
{
	float4 positionCS : SV_POSITION;
	float4 color : COLOR;
};

// Cube edge table: 12 edges, each as a pair of corner indices in [0,7].
// Corner index encoding: bit 0 = x, bit 1 = y, bit 2 = z. Component is 0 if bit clear, 1 if bit set.
static const uint cubeEdgeIndices[24] = {
	// Bottom face (z = 0).
	0, 1,  1, 3,  3, 2,  2, 0,
	// Top face (z = 1).
	4, 5,  5, 7,  7, 6,  6, 4,
	// Vertical edges.
	0, 4,  1, 5,  2, 6,  3, 7
};

// Returns the position of cube corner [0,7] in unit space [-0.5, 0.5].
float3 GetCubeCornerCentered(uint cornerIndex)
{
	return float3(
		(cornerIndex & 1) ? 0.5 : -0.5,
		(cornerIndex & 2) ? 0.5 : -0.5,
		(cornerIndex & 4) ? 0.5 : -0.5);
}

// Returns the position of cube corner [0,7] in unit space [0, 1]. Used to lerp between AABB min/max.
float3 GetCubeCornerUnit(uint cornerIndex)
{
	return float3(
		(cornerIndex & 1) ? 1.0 : 0.0,
		(cornerIndex & 2) ? 1.0 : 0.0,
		(cornerIndex & 4) ? 1.0 : 0.0);
}

[RootSignature(RS)]
PSInput VSCube(VSInput input)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	StructuredBuffer<DebugCube> cubes = ResourceDescriptorHeap[bindData.shapeBuffer];
    DebugCube cube = cubes[input.instanceId];

    uint cornerIdx = cubeEdgeIndices[input.vertexId];
	float3 unit = GetCubeCornerCentered(cornerIdx);

	float4 worldPos = mul(float4(unit, 1.0), cube.transform);

	PSInput output;
	output.positionCS = mul(mul(worldPos, camera.view), camera.projection);
	output.color = cube.color;
	return output;
}

[RootSignature(RS)]
PSInput VSSphere(VSInput input)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	StructuredBuffer<DebugSphere> spheres = ResourceDescriptorHeap[bindData.shapeBuffer];
    DebugSphere sphere = spheres[input.instanceId];

	// Each line segment uses 2 vertices. Segments wrap around to form closed circles.
    uint segmentIndex = input.vertexId / 2;  // [0, SPHERE_GREAT_CIRCLES * SPHERE_SEGMENTS).
    uint endpoint = input.vertexId & 1;  // 0 or 1.
	uint circleIdx = segmentIndex / SPHERE_SEGMENTS;
	uint segmentInCircle = segmentIndex % SPHERE_SEGMENTS;
	uint angleIndex = segmentInCircle + endpoint;

	const float twoPi = 6.28318530718;
	float angle = (float)angleIndex * twoPi / (float)SPHERE_SEGMENTS;
	float ca = cos(angle);
	float sa = sin(angle);

	float3 unit;
	if (circleIdx == 0)        unit = float3(ca, sa, 0.0);  // XY plane.
	else if (circleIdx == 1)   unit = float3(ca, 0.0, sa);  // XZ plane.
	else                       unit = float3(0.0, ca, sa);  // YZ plane.

	float3 worldPos = sphere.center + unit * sphere.radius;

	PSInput output;
	output.positionCS = mul(mul(float4(worldPos, 1.0), camera.view), camera.projection);
	output.color = sphere.color;
	return output;
}

// #TODO: Support froxel debug rendering.
/*
[RootSignature(RS)]
PSInput VSFroxel(VSInput input)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	// Cluster bounds are computed in view space by Clusters/ClusterBounds.hlsl and stored in this buffer.
	StructuredBuffer<AABB> clusterBounds = ResourceDescriptorHeap[bindData.shapeBuffer];
    AABB bounds = clusterBounds[input.instanceId];

	uint cornerIdx = cubeEdgeIndices[input.vertexId];
	float3 unit01 = GetCubeCornerUnit(cornerIdx);

	// Lerp between view-space AABB extents to position the corner.
	float3 viewPos = lerp(bounds.min.xyz, bounds.max.xyz, unit01);

	PSInput output;
	output.positionCS = mul(float4(viewPos, 1.0), camera.projection);
	output.color = ;
	return output;
}*/

[RootSignature(RS)]
float4 PSMain(PSInput input) : SV_Target
{
	return input.color;
}
