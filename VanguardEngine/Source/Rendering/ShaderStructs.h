// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>

// #TODO: Shader interop or code gen.

// Per-entity data.
struct EntityInstance
{
	XMMATRIX worldMatrix;
};

struct Camera
{
	XMFLOAT4 position;  // World space.
	XMMATRIX view;
	XMMATRIX projection;
	XMMATRIX inverseView;
	XMMATRIX inverseProjection;
	float nearPlane;
	float farPlane;
	float fieldOfView;  // Horizontal, radians.
	float aspectRatio;
};

struct MaterialData
{
	uint32_t baseColor;
	uint32_t metallicRoughness;
	uint32_t normal;
	uint32_t occlusion;
	uint32_t emissive;
	XMFLOAT3 emissiveFactor;
	XMFLOAT4 baseColorFactor;
	float metallicFactor;
	float roughnessFactor;
	XMFLOAT2 padding;
};

struct Light
{
	XMFLOAT3 position;
	uint32_t type;
	XMFLOAT3 color;
	float luminance;
	XMFLOAT3 direction;
	float padding;
};

static const uint32_t vertexChannelPosition = 0;
static const uint32_t vertexChannelNormal = 1;
static const uint32_t vertexChannelTexcoord = 2;
static const uint32_t vertexChannelTangent = 3;
static const uint32_t vertexChannelBitangent = 4;
static const uint32_t vertexChannelColor = 5;
static const uint32_t vertexChannels = 6;

struct uint128_t
{
	uint64_t low;
	uint64_t high;

	// The upper 96 bits are unused for the time being, since this struct is only used for arrays.
	uint128_t& operator=(uint32_t other) { low = static_cast<uint64_t>(other); return *this; }
};

struct VertexMetadata
{
	uint32_t activeChannels;  // Bit mask of vertex attributes.
	uint32_t padding[3];
	uint128_t channelStrides[vertexChannels];
	uint128_t channelOffsets[vertexChannels];
};