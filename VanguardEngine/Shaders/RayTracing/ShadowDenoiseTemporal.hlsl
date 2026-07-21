// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"

// Texture format is: (visibility, viewSpaceZ)
// #TODO: Camera-only reprojection ghosts on moving objects. Eventually support motion vectors.

struct BindData
{
	uint currentTexture;
	uint historyTexture;
	uint outputTexture;
	uint depthTexture;
	uint cameraBuffer;
	uint cameraIndex;
	uint historyValid;
	float blendFactor;  // Weight of the current frame sample when history is valid.
	uint2 outputResolution;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	if (dispatchId.x >= bindData.outputResolution.x || dispatchId.y >= bindData.outputResolution.y)
	{
		return;
	}

	Texture2D<float2> currentTexture = ResourceDescriptorHeap[bindData.currentTexture];
	Texture2D<float2> historyTexture = ResourceDescriptorHeap[bindData.historyTexture];
	RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[bindData.outputTexture];
	Texture2D<float> depthTexture = ResourceDescriptorHeap[bindData.depthTexture];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];

	Camera camera = cameraBuffer[bindData.cameraIndex];

	const float depth = depthTexture.Load(int3(dispatchId.xy, 0));
	const float2 current = currentTexture.Load(int3(dispatchId.xy, 0));

	// Sky pixels don't accumulate.
	if (depth <= 0.f)
	{
		outputTexture[dispatchId.xy] = float2(current.x, 0.f);
		return;
	}

	const float2 uv = (dispatchId.xy + 0.5f) / bindData.outputResolution;
	const float3 worldPosition = ReconstructWorldPosition(camera, uv, depth);
	const float viewZ = mul(float4(worldPosition, 1.f), camera.view).z;

	float visibility = current.x;

	if (bindData.historyValid)
	{
		// Reproject into the previous frame.
		float4 previousClip = mul(float4(worldPosition, 1.f), mul(camera.lastFrameView, camera.lastFrameProjection));
		previousClip /= previousClip.w;
		const float2 previousUv = ClipSpaceToUv(previousClip);

		if (all(previousUv >= 0.f) && all(previousUv <= 1.f))
		{
			const float2 history = historyTexture.SampleLevel(bilinearClamp, previousUv, 0);
			const float previousViewZ = mul(float4(worldPosition, 1.f), camera.lastFrameView).z;

			// Reject history on depth mismatch (disocclusion).
			if (abs(history.y - previousViewZ) <= 0.05f * abs(previousViewZ))
			{
				visibility = lerp(history.x, current.x, bindData.blendFactor);
			}
		}
	}

	outputTexture[dispatchId.xy] = float2(visibility, viewZ);
}
