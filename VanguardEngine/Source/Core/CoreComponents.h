// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct TransformComponent
{
	XMFLOAT3 Scale{ 1.f, 1.f, 1.f };
	XMFLOAT3 Rotation{ 0.f, 0.f, 0.f };
	XMFLOAT3 Translation{ 0.f, 0.f, 0.f };
};