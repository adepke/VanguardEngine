// Copyright (c) 2019-2021 Andrew Depke

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

struct Light
{
	XMFLOAT3 position;
	uint32_t type;
	XMFLOAT3 color;
	float luminance;
};

static const uint32_t vertexChannelPosition = 0;
static const uint32_t vertexChannelNormal = 1;
static const uint32_t vertexChannelTexcoord = 2;
static const uint32_t vertexChannelTangent = 3;
static const uint32_t vertexChannelBitangent = 4;
static const uint32_t vertexChannelColor = 5;
static const uint32_t vertexChannels = 6;

struct VertexMetadata
{
	uint32_t activeChannels;  // Bit mask of vertex attributes.
	uint32_t padding[3];
	uint32_t channelStrides[vertexChannels];
	uint32_t channelOffsets[vertexChannels];
};