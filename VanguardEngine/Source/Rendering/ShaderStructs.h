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