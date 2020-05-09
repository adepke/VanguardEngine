#include "Default_RS.hlsli"

#pragma pack_matrix(row_major)

// #TODO: Replace with ConstantBuffer in DXC branch.
cbuffer PerObject : register(b0)
{
	matrix WorldMatrix;
};

// #TODO: Replace with ConstantBuffer in DXC branch.
cbuffer CameraBuffer : register(b1)
{
	matrix ViewMatrix;
	matrix ProjectionMatrix;
};

struct Vertex
{
	float3 Position : POSITION;
	float4 Color : COLOR;
};

StructuredBuffer<Vertex> VertexBuffer : register(t0);

struct Output
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

[RootSignature(RS)]
Output main(uint VertexID : SV_VertexID)
{
	Vertex vertex = VertexBuffer[VertexID];

	Output Out;
	Out.Position = float4(vertex.Position, 1.f);
	Out.Position = mul(Out.Position, WorldMatrix);
	Out.Position = mul(Out.Position, ViewMatrix);
	Out.Position = mul(Out.Position, ProjectionMatrix);
	Out.Color = vertex.Color;

	return Out;
}