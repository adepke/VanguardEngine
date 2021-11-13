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
ByteAddressBuffer vertexPositionBuffer : register(t0, space2);
ByteAddressBuffer vertexExtraBuffer : register(t1, space2);

struct VertexMetadata
{
    uint activeChannels;  // Bit mask of vertex attributes.
    uint channelStrides[vertexChannels];
};

ConstantBuffer<VertexMetadata> vertexMetadata : register(b0, space2);

bool HasVertexAttribute(uint channel)
{
    return vertexMetadata.activeChannels & (1 << channel);
}

float3 LoadVertexPosition(uint vertexId)
{
    return HasVertexAttribute(vertexChannelPosition) ? vertexPositionBuffer.Load3(vertexId * vertexMetadata.channelStrides[vertexChannelPosition]) : float3(0, 0, 0);
}

float3 LoadVertexNormal(uint vertexId)
{
    return HasVertexAttribute(vertexChannelNormal) ? vertexExtraBuffer.Load3(vertexId * vertexMetadata.channelStrides[vertexChannelNormal]) : float3(0, 0, 0);
}

float2 LoadVertexTexcoord(uint vertexId)
{
    return HasVertexAttribute(vertexChannelTexcoord) ? vertexExtraBuffer.Load2(vertexId * vertexMetadata.channelStrides[vertexChannelTexcoord]) : float2(0, 0);
}

float4 LoadVertexTangent(uint vertexId)
{
    return HasVertexAttribute(vertexChannelTangent) ? vertexExtraBuffer.Load4(vertexId * vertexMetadata.channelStrides[vertexChannelTangent]) : float4(0, 0, 0, 0);
}

float4 LoadVertexBitangent(uint vertexId)
{
    return HasVertexAttribute(vertexChannelBitangent) ? vertexExtraBuffer.Load4(vertexId * vertexMetadata.channelStrides[vertexChannelBitangent]) : float4(0, 0, 0, 0);
}

float4 LoadVertexColor(uint vertexId)
{
    return HasVertexAttribute(vertexChannelColor) ? vertexExtraBuffer.Load4(vertexId * vertexMetadata.channelStrides[vertexChannelColor]) : float4(0, 0, 0, 0);
}

#endif  // __VERTEXASSEMBLY_HLSLI__