#include "Default_RS.hlsli"

#pragma pack_matrix(row_major)

struct PerObject
{
	matrix WorldMatrix;
};

ConstantBuffer<PerObject> perObject : register(b0);

struct CameraBuffer
{
	matrix ViewMatrix;
	matrix ProjectionMatrix;
};

ConstantBuffer<CameraBuffer> cameraBuffer : register(b1);

struct Vertex
{
	float3 Position : POSITION;
	float4 Color : COLOR;
};

StructuredBuffer<Vertex> vertexBuffer : register(t0);

struct Output
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

[RootSignature(RS)]
Output main(uint VertexID : SV_VertexID)
{
	Vertex vertex = vertexBuffer[VertexID];

	Output Out;
	Out.Position = float4(vertex.Position, 1.f);
	Out.Position = mul(Out.Position, perObject.WorldMatrix);
	Out.Position = mul(Out.Position, cameraBuffer.ViewMatrix);
	Out.Position = mul(Out.Position, cameraBuffer.ProjectionMatrix);
	Out.Color = vertex.Color;

	return Out;
}