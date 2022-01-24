// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <DirectXMath.h>

#include <string>

using namespace DirectX;

struct NameComponent
{
	std::string name;
};

struct TransformComponent
{
	XMFLOAT3 scale{ 1.f, 1.f, 1.f };
	XMFLOAT3 rotation{ 0.f, 0.f, 0.f };
	XMFLOAT3 translation{ 0.f, 0.f, 0.f };
};

// Empty for now, used to tag entities that are being controlled.
struct ControlComponent {};