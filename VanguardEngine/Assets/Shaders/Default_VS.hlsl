#include "Default_RS.hlsli"

#pragma pack_matrix(row_major)

struct PerObject
{
	matrix worldMatrix;
};

ConstantBuffer<PerObject> perObject : register(b0);

struct CameraBuffer
{
	matrix viewMatrix;
	matrix projectionMatrix;
};

ConstantBuffer<CameraBuffer> cameraBuffer : register(b1);

struct Vertex
{
	float3 position : POSITION;  // Object space.
	float3 normal : NORMAL;  // Object space.
	float2 uv : UV;
	float3 tangent : TANGENT;  // Object space.
	float3 bitangent : BITANGENT;  // Object space.
};

StructuredBuffer<Vertex> vertexBuffer : register(t0);

struct Output
{
	float4 positionCS : SV_POSITION;  // Clip space.
	float3 position : POSITION;  // World space.
	float3 normal : NORMAL;  // World space.
	float2 uv : UV;
	float3 tangent : TANGENT;  // World space.
	float3 bitangent : BITANGENT;  // World space.
};

[RootSignature(RS)]
Output main(uint vertexID : SV_VertexID)
{
	Vertex vertex = vertexBuffer[vertexID];

	Output output;
	output.positionCS = float4(vertex.position, 1.f);
	output.positionCS = mul(output.positionCS, perObject.worldMatrix);
	output.positionCS = mul(output.positionCS, cameraBuffer.viewMatrix);
	output.positionCS = mul(output.positionCS, cameraBuffer.projectionMatrix);
	output.position = mul(float4(vertex.position, 1.f), perObject.worldMatrix).xyz;
	output.normal = normalize(mul(float4(vertex.normal, 0.f), perObject.worldMatrix)).xyz;
	output.uv = vertex.uv;
	output.tangent = normalize(mul(float4(vertex.tangent, 0.f), perObject.worldMatrix)).xyz;
	output.bitangent = normalize(mul(float4(vertex.bitangent, 0.f), perObject.worldMatrix)).xyz;

	return output;
}