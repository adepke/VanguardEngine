// Copyright (c) 2019-2021 Andrew Depke

#ifndef __VERTEXASSEMBLY_HLSLI__
#define __VERTEXASSEMBLY_HLSLI__

static const uint vertexChannelPosition = 0;
static const uint vertexChannelNormal = 1;
static const uint vertexChannelTexcoord = 2;
static const uint vertexChannelTangent = 3;
static const uint vertexChannelBitangent = 4;
static const uint vertexChannelColor = 5;
static const uint vertexChannels = 6;

// Vertex data is stored in two separate buffers.
ByteAddressBuffer vertexPositionBuffer : register(t0, space3);
ByteAddressBuffer vertexExtraBuffer : register(t1, space3);

struct VertexMetadata
{
	uint activeChannels;  // Bit mask of vertex attributes.
	float3 padding;
	// All entries are really uint4's.
	uint channelStrides[vertexChannels];
	uint channelOffsets[vertexChannels];
};

ConstantBuffer<VertexMetadata> vertexMetadata : register(b0, space3);

bool HasVertexAttribute(uint channel)
{
	return vertexMetadata.activeChannels & (1 << channel);
}

float4 LoadVertexPosition(uint vertexId)
{
	return HasVertexAttribute(vertexChannelPosition) ? float4(vertexPositionBuffer.Load<float3>(vertexId * vertexMetadata.channelStrides[vertexChannelPosition] + vertexMetadata.channelOffsets[vertexChannelPosition]), 1) : float4(0, 0, 0, 1);
}

float3 LoadVertexNormal(uint vertexId)
{
	return HasVertexAttribute(vertexChannelNormal) ? vertexExtraBuffer.Load<float3>(vertexId * vertexMetadata.channelStrides[vertexChannelNormal] + vertexMetadata.channelOffsets[vertexChannelNormal]) : float3(0, 0, 0);
}

float2 LoadVertexTexcoord(uint vertexId)
{
	return HasVertexAttribute(vertexChannelTexcoord) ? vertexExtraBuffer.Load<float2>(vertexId * vertexMetadata.channelStrides[vertexChannelTexcoord] + vertexMetadata.channelOffsets[vertexChannelTexcoord]) : float2(0, 0);
}

float4 LoadVertexTangent(uint vertexId)
{
	return HasVertexAttribute(vertexChannelTangent) ? vertexExtraBuffer.Load<float4>(vertexId * vertexMetadata.channelStrides[vertexChannelTangent] + vertexMetadata.channelOffsets[vertexChannelTangent]) : float4(0, 0, 0, 0);
}

float4 LoadVertexBitangent(uint vertexId)
{
	if (HasVertexAttribute(vertexChannelBitangent))
	{
		return vertexExtraBuffer.Load<float4>(vertexId * vertexMetadata.channelStrides[vertexChannelBitangent] + vertexMetadata.channelOffsets[vertexChannelBitangent]);
	}
	
	else
	{
		float3 normal = LoadVertexNormal(vertexId);
		float3 tangent = LoadVertexTangent(vertexId).xyz;
		return float4(cross(normal, tangent), 1.f);
	}
}

float4 LoadVertexColor(uint vertexId)
{
	return HasVertexAttribute(vertexChannelColor) ? vertexExtraBuffer.Load<float4>(vertexId * vertexMetadata.channelStrides[vertexChannelColor] + vertexMetadata.channelOffsets[vertexChannelColor]) : float4(0, 0, 0, 0);
}

#endif  // __VERTEXASSEMBLY_HLSLI__