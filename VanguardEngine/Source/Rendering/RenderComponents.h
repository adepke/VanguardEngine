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
struct MeshComponent
{
	struct Subset
	{
		PrimitiveOffset localOffset;
		size_t vertices;
		size_t indices;
		size_t materialIndex;
		XMFLOAT4X4 transform;  // Mesh-local space.
		XMFLOAT3 boundingSphereCenter;  // Subset-local space.
		float boundingSphereRadius;
	};

	std::vector<Subset> subsets;

	PrimitiveOffset globalOffset;

	VertexMetadata metadata;
};

// Tracks a GPU scene allocation.
// Do not serialize this component, it is managed at runtime by the renderer.
// Do not copy this component, it must be unique per instance.
struct GpuSlotComponent
{
	uint32_t baseSlot = 0;
	uint32_t count = 0;
};

// Tag for entities that have a dirty transform in the GPU scene and must be re-uploaded.
struct TransformDirtyComponent {};

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

	LightType type = LightType::Point;
	XMFLOAT3 color = { 1.f, 1.f, 1.f };
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

	float solarZenithAngle = 0.f;
	float speed = 0.f;
	TimeOfDayAnimation animation = TimeOfDayAnimation::Static;
};

// Considered using EnTT registry context for singleton-like behavior, but this context doesn't
// serialize. So instead, using a component like other systems.
struct WeatherComponent
{
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(WeatherComponent, coverage, precipitation, windStrength, windDirection);

	float coverage = 0.f;
	float precipitation = 0.f;
	float windStrength = 0.f;
	XMFLOAT2 windDirection = { 0.f, 0.f };
};
