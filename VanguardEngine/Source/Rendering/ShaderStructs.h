// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <Core/Windows/DirectX12Minimal.h>

// #TODO: Shader interop or code gen.

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
	uint32_t values[4];

	uint128_t& operator=(uint32_t other) { values[0] = other; return *this; }
	uint32_t& operator[](int idx) { return values[idx]; }
	void AddAll(uint32_t value) { std::for_each(std::begin(values), std::end(values), [value](auto& v) { v += value; }); }
};

struct VertexMetadata
{
	uint32_t activeChannels;  // Bit mask of vertex attributes.
	uint32_t padding[3];
	uint128_t channelStrides[vertexChannels / 4 + 1];
	uint128_t channelOffsets[vertexChannels / 4 + 1];
};

struct VertexAssemblyData
{
	uint32_t positionBuffer;
	uint32_t extraBuffer;
	XMFLOAT2 padding;
	VertexMetadata metadata;
};

struct ClusterData
{
	uint32_t lightListBuffer;
	uint32_t lightInfoBuffer;
	float logY;
	uint32_t padding1;
	uint32_t dimensions[3];
	uint32_t padding2;
};

struct IblData
{
	uint32_t irradianceTexture;
	uint32_t prefilterTexture;
	uint32_t brdfTexture;
	uint32_t prefilterLevels;
};

struct ObjectData
{
	XMMATRIX worldMatrix;
	VertexMetadata vertexMetadata;
	uint32_t materialIndex;
	float boundingSphereRadius;
	XMFLOAT2 padding;
};

struct MeshIndirectArgument
{
	uint32_t batchId;
	D3D12_DRAW_INDEXED_ARGUMENTS draw;
	XMFLOAT2 padding;
};