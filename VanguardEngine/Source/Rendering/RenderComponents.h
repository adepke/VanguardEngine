// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/ShaderStructs.h>

// #TODO: Huge header leakage for serialization, json_fwd doesn't help here either.
#include <json.hpp>

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

// Do not serialize this component, it is rebuilt at runtime from an AssetComponent.
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
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(CameraComponent, nearPlane, farPlane, fieldOfView);

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
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(LightComponent, type, color);

	LightType type;
	XMFLOAT3 color;
};

enum class TimeOfDayAnimation
{
	Static,
	Cycle,
	Oscillate
};

struct TimeOfDayComponent
{
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(TimeOfDayComponent, solarZenithAngle, speed, animation);

	float solarZenithAngle;
	float speed;
	TimeOfDayAnimation animation;
};