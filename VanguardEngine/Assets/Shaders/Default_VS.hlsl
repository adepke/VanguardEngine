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
	float3 Position : POSITION;  // Object space.
	float3 Normal : NORMAL;  // Object space.
	float2 UV : UV;
	float3 Tangent : TANGENT;  // Object space.
	float3 Bitangent : BITANGENT;  // Object space.
};

StructuredBuffer<Vertex> vertexBuffer : register(t0);

struct Output
{
	float4 PositionCS : SV_POSITION;  // Clip space.
	float3 Position : POSITION;  // World space.
	float3 Normal : NORMAL;  // World space.
	float2 UV : UV;
	float3 Tangent : TANGENT;  // World space.
	float3 Bitangent : BITANGENT;  // World space.
};

[RootSignature(RS)]
Output main(uint VertexID : SV_VertexID)
{
	Vertex vertex = vertexBuffer[VertexID];

	Output Out;
	Out.PositionCS = float4(vertex.Position, 1.f);
	Out.PositionCS = mul(Out.PositionCS, perObject.WorldMatrix);
	Out.PositionCS = mul(Out.PositionCS, cameraBuffer.ViewMatrix);
	Out.PositionCS = mul(Out.PositionCS, cameraBuffer.ProjectionMatrix);
	Out.Position = mul(float4(vertex.Position, 1.f), perObject.WorldMatrix).xyz;
	Out.Normal = normalize(mul(float4(vertex.Normal, 0.f), perObject.WorldMatrix)).xyz;
	Out.UV = vertex.UV;
	Out.Tangent = normalize(mul(float4(vertex.Tangent, 0.f), perObject.WorldMatrix)).xyz;
	Out.Bitangent = normalize(mul(float4(vertex.Bitangent, 0.f), perObject.WorldMatrix)).xyz;

	return Out;
}