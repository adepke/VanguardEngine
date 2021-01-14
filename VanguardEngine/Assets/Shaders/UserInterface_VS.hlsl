// Copyright (c) 2019-2021 Andrew Depke

#include "UserInterface_RS.hlsli"

struct Projections
{
	float4x4 projectionMatrix;
};

ConstantBuffer<Projections> projections : register(b0);

struct Vertex
{
	float2 position : POSITION;
	float2 uv : TEXCOORD0;
	uint color : COLOR0;
};

StructuredBuffer<Vertex> vertexBuffer : register(t0);

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

[RootSignature(RS)]
PS_INPUT main(uint vertexID : SV_VertexID)
{
	Vertex vertex = vertexBuffer[vertexID];

	PS_INPUT output;
	output.position = mul(projections.projectionMatrix, float4(vertex.position.xy, 0.f, 1.f));
	output.color.r = float((vertex.color >> 0) & 0xFF) / 255.f;
	output.color.g = float((vertex.color >> 8) & 0xFF) / 255.f;
	output.color.b = float((vertex.color >> 16) & 0xFF) / 255.f;
	output.color.a = float((vertex.color >> 24) & 0xFF) / 255.f;
	output.uv = vertex.uv;
	
	return output;
}