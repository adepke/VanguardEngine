// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/ShaderStructs.h>

#include <vector>

struct PrimitiveOffset
{
	size_t index = 0;
	size_t position = 0;
	size_t extra = 0;

	PrimitiveOffset operator+(const PrimitiveOffset& other) const
	{
		return { index + other.index, position + other.position, extra + other.extra };
	}

	PrimitiveOffset& operator+=(const PrimitiveOffset& other)
	{
		*this = *this + other;
		return *this;
	}
};

// #TODO: Array of mesh materials bound to vertex/index offsets to enable multiple materials per mesh.
struct MeshComponent
{
	struct Subset
	{
		PrimitiveOffset localOffset;
		size_t indices;
		size_t materialIndex;
		float boundingSphereRadius;
	};

	std::vector<Subset> subsets;

	PrimitiveOffset globalOffset;

	VertexMetadata metadata;
};

struct CameraComponent
{
	float nearPlane = 0.1f;
	float farPlane = 10000.f;
	float fieldOfView = 1.57079633f;  // 90 Degrees.
};

enum class LightType
{
	Point,
	Directional
};

struct LightComponent
{
	LightType type;
	XMFLOAT3 color;
};