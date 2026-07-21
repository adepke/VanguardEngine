// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Constants.hlsli"
#include "Math.hlsli"
#include "Utils/OctahedralNormals.hlsli"

struct BindData
{
	uint depthTexture;
	uint normalTexture;
	uint outputTexture;
	uint accelerationStructure;
	uint cameraBuffer;
	uint cameraIndex;
	uint2 outputResolution;
	uint blueNoiseTexture;
	float3 sunDirection;  // Towards the sun, normalized.
	uint timeSlice;
};

ConstantBuffer<BindData> bindData : register(b0);

// Uniformly samples a direction within a cone around +Z, mapped onto the given axis.
float3 SampleCone(float3 axis, float angularRadius, float2 random)
{
	const float cosThetaMax = cos(angularRadius);
	const float cosTheta = 1.f - random.x * (1.f - cosThetaMax);
	const float sinTheta = sqrt(saturate(1.f - cosTheta * cosTheta));
	const float phi = 2.f * pi * random.y;

	const float3 local = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

	return normalize(mul(local, ComputeOrthonormalBasis(axis)));
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	if (dispatchId.x >= bindData.outputResolution.x || dispatchId.y >= bindData.outputResolution.y)
	{
		return;
	}

	Texture2D<float> depthTexture = ResourceDescriptorHeap[bindData.depthTexture];
	RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];

	const float depth = depthTexture.Load(int3(dispatchId.xy, 0));

	// Sky pixels (inverse depth buffer far plane) receive full visibility.
	if (depth <= 0.f)
	{
		outputTexture[dispatchId.xy] = float2(1.f, -1.f);
		return;
	}

	Texture2D<float2> normalTexture = ResourceDescriptorHeap[bindData.normalTexture];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Texture2D<float4> blueNoiseTexture = ResourceDescriptorHeap[bindData.blueNoiseTexture];
	RaytracingAccelerationStructure accelerationStructure = ResourceDescriptorHeap[bindData.accelerationStructure];

	Camera camera = cameraBuffer[bindData.cameraIndex];

	const float2 uv = (dispatchId.xy + 0.5f) / bindData.outputResolution;
	const float3 worldPosition = ReconstructWorldPosition(camera, uv, depth);
	const float3 normal = DecodeNormalOctahedral(normalTexture.Load(int3(dispatchId.xy, 0)));

	// Offset the ray origin along the surface normal to avoid self intersection, scaled with
	// distance to compensate for the loss of depth precision.
	const float viewDistance = length(worldPosition - camera.position.xyz);
	const float3 rayOrigin = worldPosition + normal * max(0.01f, 0.002f * viewDistance);

	// Animated blue noise drives the sun disk sample, the temporal denoiser integrates across frames.
	uint blueNoiseWidth, blueNoiseHeight;
	blueNoiseTexture.GetDimensions(blueNoiseWidth, blueNoiseHeight);
	const uint2 noiseOffset = uint2(frac(bindData.timeSlice * float2(0.7548776662f, 0.5698402909f)) * float2(blueNoiseWidth, blueNoiseHeight));  // R2 sequence.
	const float2 random = blueNoiseTexture.Load(int3((dispatchId.xy + noiseOffset) % uint2(blueNoiseWidth, blueNoiseHeight), 0)).rg;

	const float3 rayDirection = SampleCone(bindData.sunDirection, sunAngularRadius, random);

	// Backfacing pixels are fully shadowed, don't waste a ray.
	if (dot(normal, rayDirection) <= 0.f)
	{
		outputTexture[dispatchId.xy] = float2(0.f, 0.f);
		return;
	}

	RayDesc ray;
	ray.Origin = rayOrigin;
	ray.Direction = rayDirection;
	ray.TMin = 0.f;
	ray.TMax = 100000.f;

	// Shadow rays only need any hit, not the closest. All geometry is currently opaque.
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;
	query.TraceRayInline(accelerationStructure, RAY_FLAG_NONE, 0xFF, ray);
	query.Proceed();

	const bool hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;

	// Store visibility and hit distance.
	outputTexture[dispatchId.xy] = float2(hit ? 0.f : 1.f, hit ? query.CommittedRayT() : -1.f);
}
