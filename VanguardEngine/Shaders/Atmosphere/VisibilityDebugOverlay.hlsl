// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Color.hlsli"
#include "Constants.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Volumetrics/VisibilityMoments.hlsli"

// Atmosphere visibility is stored as shadow moments. Visualize as segments.

#define DEBUG_MODE_SHADOW_START 0
#define DEBUG_MODE_SHADOW_LENGTH 1
#define DEBUG_MODE_COMBINED 2

struct BindData
{
	uint visibilityTexture;
	uint cameraBuffer;
	uint cameraIndex;
	uint mode;
	float shadowRange;  // Kilometers, color ramp top.
};

ConstantBuffer<BindData> bindData : register(b0);

struct PixelIn
{
	float4 positionCS : SV_POSITION;  // Clip space.
	float2 uv : TEXCOORD0;
};

[RootSignature(RS)]
PixelIn VSMain(uint vertexID : SV_VertexID)
{
	PixelIn output;
	float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
	output.positionCS = float4((uv.x - 0.5) * 2.0, -(uv.y - 0.5) * 2.0, 0, 1);  // Z of 0 due to the inverse depth.
	output.uv = uv;

	return output;
}

[RootSignature(RS)]
float4 PSMain(PixelIn input) : SV_Target
{
	Texture2D<float2> visibilityTexture = ResourceDescriptorHeap[bindData.visibilityTexture];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	float2 visibilityMoments = visibilityTexture.Sample(bilinearClamp, input.uv);
	float2 shadowSegment = VisibilityMomentsToSegment(visibilityMoments);
	const float shadowStart = shadowSegment.x;
	const float shadowLength = shadowSegment.y;
	
	if (shadowLength < 0.0001f)
	{
		return float4(0.xxx, 1.f);
	}

	const float range = max(bindData.shadowRange, 0.0001f);
	float3 output = 0.xxx;

	switch (bindData.mode)
	{
	case DEBUG_MODE_SHADOW_START:
	{
		// Where along the view ray the occluded segment begins. Near the camera is green, far is red.
		output = MapToRainbow(saturate(shadowStart / range));
		break;
	}
	case DEBUG_MODE_SHADOW_LENGTH:
	{
		// How much in-scattering is omitted. This is the term that actually drives shaft intensity.
		output = MapToRainbow(saturate(shadowLength / range));
		break;
	}
	case DEBUG_MODE_COMBINED:
	{
		// Hue encodes the segment start, brightness encodes the segment length. Useful for spotting
		// discontinuities where a long shadow abruptly jumps to a different depth.
		output = MapToRainbow(saturate(shadowStart / range)) * saturate(shadowLength / range);
		break;
	}
	default: break;
	}

	return float4(output, 1.f);
}
