// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <DirectXMath.h>
// #TODO: Huge header leakage for serialization, json_fwd doesn't help here either.
#include <json.hpp>

#include <string>

using namespace DirectX;

struct NameComponent
{
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(NameComponent, name);

	std::string name;
};

struct TransformComponent
{
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransformComponent, scale, rotation, translation);

	XMFLOAT3 scale{ 1.f, 1.f, 1.f };
	XMFLOAT3 rotation{ 0.f, 0.f, 0.f };
	XMFLOAT3 translation{ 0.f, 0.f, 0.f };
};

// Empty for now, used to tag entities that are being controlled.
struct ControlComponent {};