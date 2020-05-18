#include "UserInterface_RS.hlsli"

struct Projections
{
	float4x4 ProjectionMatrix;
};

ConstantBuffer<Projections> projections : register(b0);

struct Vertex
{
	float2 pos : POSITION;
	float2 uv : TEXCOORD0;
	uint col : COLOR0;
};

StructuredBuffer<Vertex> VertexBuffer : register(t0);

struct PS_INPUT
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

[RootSignature(RS)]
PS_INPUT main(uint VertexID : SV_VertexID)
{
	Vertex vertex = VertexBuffer[VertexID];

	PS_INPUT output;
	output.pos = mul(projections.ProjectionMatrix, float4(vertex.pos.xy, 0.f, 1.f));
	output.col.r = float((vertex.col >> 0) & 0xFF) / 255.f;
	output.col.g = float((vertex.col >> 8) & 0xFF) / 255.f;
	output.col.b = float((vertex.col >> 16) & 0xFF) / 255.f;
	output.col.a = float((vertex.col >> 24) & 0xFF) / 255.f;
	output.uv = vertex.uv;
	
	return output;
}