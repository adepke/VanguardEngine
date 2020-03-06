// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <optional>

// Reduced resource descriptions, the render graph will automatically determine the bind flags, access flags, etc.

struct RGBufferDescription
{
	ResourceFrequency UpdateRate;
	size_t Size;  // Element count. Size * Stride = Byte count.
	size_t Stride;
	std::optional<DXGI_FORMAT> Format;
};

struct RGTextureDescription
{
	ResourceFrequency UpdateRate;
	uint32_t Width = 1;
	uint32_t Height = 1;
	uint32_t Depth = 1;
	DXGI_FORMAT Format;
};