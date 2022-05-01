// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"

struct BindData
{
	uint hiZTexture;
	uint cameraBuffer;
	uint cameraIndex;
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
	Texture2D<float> hiZTexture = ResourceDescriptorHeap[bindData.hiZTexture];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	float depth = hiZTexture.Sample(pointClamp, input.uv);
	float4 output = float4(depth, 0.f, 0.f, 1.f);
	output.x = 1.f - LinearizeDepth(camera, output.x);

	return output;
}