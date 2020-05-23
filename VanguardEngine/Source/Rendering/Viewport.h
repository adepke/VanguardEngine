// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Windows/DirectX12Minimal.h>

struct Viewport
{
	float PositionX;
	float PositionY;
	float Width;
	float Height;

	operator D3D12_VIEWPORT() const { return D3D12_VIEWPORT{ PositionX, PositionY, Width, Height, 0.f, 1.f }; }
};