// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Color.hlsli"

struct Data
{
	float4x4 projectionMatrix;
	uint cameraBuffer;
	uint vertexBuffer;
	uint vertexOffset;
	uint texture;
	uint depthLinearization;
};

ConstantBuffer<Data> data : register(b0);

struct Vertex
{
	float2 position : POSITION;
	float2 uv : TEXCOORD0;
	uint color : COLOR0;
};

struct VertexIn
{
	uint vertexID : SV_VertexID;
};

struct PixelIn
{
	float4 position : SV_POSITION;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

[RootSignature(RS)]
PixelIn VSMain(VertexIn input)
{
	StructuredBuffer<Vertex> buffer = ResourceDescriptorHeap[data.vertexBuffer];
	Vertex vertex = buffer[input.vertexID + data.vertexOffset];

	PixelIn output;
	output.position = mul(data.projectionMatrix, float4(vertex.position.xy, 0.f, 1.f));  // Post-multiply due to column major.
	output.color.r = float((vertex.color >> 0) & 0xFF) / 255.f;
	output.color.g = float((vertex.color >> 8) & 0xFF) / 255.f;
	output.color.b = float((vertex.color >> 16) & 0xFF) / 255.f;
	output.color.a = float((vertex.color >> 24) & 0xFF) / 255.f;
	output.uv = vertex.uv;

	// Convert the vertex color to linear space, this will be converted back to sRGB during presentation.
	// See https://github.com/ocornut/imgui/issues/578 and https://github.com/ocornut/imgui/issues/578#issuecomment-379467586 for details.
	output.color.xyz = SRGBToLinear(output.color.xyz);
	
	return output;
}

[RootSignature(RS)]
float4 PSMain(PixelIn input) : SV_Target
{
	Texture2D<float4> textureTarget = ResourceDescriptorHeap[data.texture];
	float4 output = input.color * textureTarget.Sample(bilinearClamp, input.uv);

	// If we're rendering a depth texture, we may want to linearize it.
	if (data.depthLinearization)
	{
		StructuredBuffer<Camera> camera = ResourceDescriptorHeap[data.cameraBuffer];
		output.xyz = 1.f - LinearizeDepth(camera[0], output.x);
		output.w = 1.f;

		// Counteract the gamma correction on depth textures during presentation.
		output.xyz = SRGBToLinear(output.xyz);
	}

	return output;
}