// Copyright (c) 2019-2022 Andrew Depke

#ifndef __VERTEXASSEMBLY_HLSLI__
#define __VERTEXASSEMBLY_HLSLI__

static const uint vertexChannelPosition = 0;
static const uint vertexChannelNormal = 1;
static const uint vertexChannelTexcoord = 2;
static const uint vertexChannelTangent = 3;
static const uint vertexChannelBitangent = 4;
static const uint vertexChannelColor = 5;
static const uint vertexChannels = 6;

struct VertexMetadata
{
	uint activeChannels;  // Bit mask of vertex attributes.
	float3 padding;
	// Tightly packed.
	uint4 channelStrides[vertexChannels / 4 + 1];
	uint4 channelOffsets[vertexChannels / 4 + 1];
};

struct VertexAssemblyData
{
    uint positionBuffer;
    uint extraBuffer;
    float2 padding;
    VertexMetadata metadata;
};

ConstantBuffer<VertexMetadata> vertexMetadata : register(b0, space3);

bool HasVertexAttribute(VertexMetadata metadata, uint channel)
{
	return metadata.activeChannels & (1u << channel);
}

uint GetVertexChannelStride(VertexMetadata metadata, uint channel)
{
	return metadata.channelStrides[channel / 4][channel % 4];
}

uint GetVertexChannelOffset(VertexMetadata metadata, uint channel)
{
	return metadata.channelOffsets[channel / 4][channel % 4];
}

uint GetVertexChannelIndex(VertexMetadata metadata, uint vertexId, uint channel)
{
	return vertexId * GetVertexChannelStride(metadata, channel) + GetVertexChannelOffset(metadata, channel);
}

float4 LoadVertexPosition(VertexAssemblyData assembly, uint vertexId)
{
    ByteAddressBuffer positions = ResourceDescriptorHeap[assembly.positionBuffer];
	
	return HasVertexAttribute(assembly.metadata, vertexChannelPosition) ?
		float4(positions.Load<float3>(GetVertexChannelIndex(assembly.metadata, vertexId, vertexChannelPosition)), 1) :
		float4(1, 1, 1, 1);
}

float3 LoadVertexNormal(VertexAssemblyData assembly, uint vertexId)
{
    ByteAddressBuffer extras = ResourceDescriptorHeap[assembly.extraBuffer];
	
	return HasVertexAttribute(assembly.metadata, vertexChannelNormal) ?
		extras.Load<float3>(GetVertexChannelIndex(assembly.metadata, vertexId, vertexChannelNormal)) :
		float3(0, 0, 0);
}

float2 LoadVertexTexcoord(VertexAssemblyData assembly, uint vertexId)
{
    ByteAddressBuffer extras = ResourceDescriptorHeap[assembly.extraBuffer];
	
	return HasVertexAttribute(assembly.metadata, vertexChannelTexcoord) ?
		extras.Load<float2>(GetVertexChannelIndex(assembly.metadata, vertexId, vertexChannelTexcoord)) :
		float2(0, 0);
}

float4 LoadVertexTangent(VertexAssemblyData assembly, uint vertexId)
{
    ByteAddressBuffer extras = ResourceDescriptorHeap[assembly.extraBuffer];
	
	return HasVertexAttribute(assembly.metadata, vertexChannelTangent) ?
		extras.Load<float4>(GetVertexChannelIndex(assembly.metadata, vertexId, vertexChannelTangent)) :
		float4(0, 0, 0, 0);
}

float4 LoadVertexBitangent(VertexAssemblyData assembly, uint vertexId)
{
    if (HasVertexAttribute(assembly.metadata, vertexChannelBitangent))
	{
        ByteAddressBuffer extras = ResourceDescriptorHeap[assembly.extraBuffer];
		
		return extras.Load<float4>(GetVertexChannelIndex(assembly.metadata, vertexId, vertexChannelBitangent));
	}
	
	else
	{
		float3 normal = LoadVertexNormal(assembly, vertexId);
		float3 tangent = LoadVertexTangent(assembly, vertexId).xyz;
		return float4(cross(normal, tangent), 1.f);
	}
}

float4 LoadVertexColor(VertexAssemblyData assembly, uint vertexId)
{
    ByteAddressBuffer extras = ResourceDescriptorHeap[assembly.extraBuffer];
	
	return HasVertexAttribute(assembly.metadata, vertexChannelColor) ?
		extras.Load<float4>(GetVertexChannelIndex(assembly.metadata, vertexId, vertexChannelColor)) :
		float4(0, 0, 0, 1);
}

#endif  // __VERTEXASSEMBLY_HLSLI__