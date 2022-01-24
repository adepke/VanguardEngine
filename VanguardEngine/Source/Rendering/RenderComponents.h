// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Material.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/ShaderStructs.h>

#include <vector>

struct PrimitiveOffset
{
	size_t index = 0;
	size_t position = 0;
	size_t extra = 0;
};

// #TODO: Array of mesh materials bound to vertex/index offsets to enable multiple materials per mesh.
struct MeshComponent
{
	struct Subset
	{
		PrimitiveOffset localOffset;
		size_t indices;

		Material material;
	};

	std::vector<Subset> subsets;

	PrimitiveOffset globalOffset;

	VertexMetadata metadata;
};

struct CameraComponent
{
	float nearPlane = 0.1f;
	float farPlane = 1000.f;
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