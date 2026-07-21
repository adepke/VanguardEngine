// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"

struct BindData
{
	uint outputTexture;
	uint accelerationStructure;
	uint cameraBuffer;
	uint cameraIndex;
	uint2 outputResolution;
	uint debugMode;  // 1=instances, 2=hit distance, 3=barycentrics.
};

ConstantBuffer<BindData> bindData : register(b0);

// Cheap integer hash to a pseudo-random color.
float3 HashColor(uint value)
{
	uint hash = value;
	hash ^= hash >> 16;
	hash *= 0x7feb352d;
	hash ^= hash >> 15;
	hash *= 0x846ca68b;
	hash ^= hash >> 16;

	return float3((hash & 0xFF) / 255.f, ((hash >> 8) & 0xFF) / 255.f, ((hash >> 16) & 0xFF) / 255.f);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	if (dispatchId.x >= bindData.outputResolution.x || dispatchId.y >= bindData.outputResolution.y)
	{
		return;
	}

	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	RaytracingAccelerationStructure accelerationStructure = ResourceDescriptorHeap[bindData.accelerationStructure];

	Camera camera = cameraBuffer[bindData.cameraIndex];

	const float2 uv = (dispatchId.xy + 0.5f) / bindData.outputResolution;

	RayDesc ray;
	ray.Origin = camera.position.xyz;
	ray.Direction = ComputeRayDirection(camera, uv);
	ray.TMin = 0.f;
	ray.TMax = 100000.f;

	RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(accelerationStructure, RAY_FLAG_NONE, 0xFF, ray);
	query.Proceed();

	float3 color = 0.xxx;

	if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
	{
		switch (bindData.debugMode)
		{
			case 1: color = HashColor(query.CommittedInstanceID() + query.CommittedInstanceIndex() * 7919); break;
			case 2: color = frac(query.CommittedRayT() * 0.01f).xxx; break;
			case 3: color = float3(query.CommittedTriangleBarycentrics(), 0.f); break;
		}
	}

	outputTexture[dispatchId.xy] = float4(color, 1.f);
}
