// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <optional>

struct TransientBufferDescription
{
	ResourceFrequency updateRate = ResourceFrequency::Dynamic;
	size_t size;  // Element count. Size * Stride = Byte count.
	size_t stride = 0;
	std::optional<DXGI_FORMAT> format;
};

struct TransientTextureDescription
{
	uint32_t width = 0;  // Will match output resolution if left at 0.
	uint32_t height = 0;
	uint32_t depth = 1;
	DXGI_FORMAT format;
};