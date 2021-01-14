// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Core/Windows/DirectX12Minimal.h>

struct Viewport
{
	float positionX;
	float positionY;
	float width;
	float height;

	operator D3D12_VIEWPORT() const { return D3D12_VIEWPORT{ positionX, positionY, width, height, 0.f, 1.f }; }
};