// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <limits>

using namespace DirectX;

// Taken from: https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
inline constexpr bool IsPowerOf2(uint32_t value)
{
	return (value & (value - 1)) == 0;  // Note: doesn't account for 0.
}

// Taken from: https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
// Doesn't step up if value is already a power of 2.
inline constexpr uint32_t NextPowerOf2(uint32_t value)
{
	--value;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	++value;

	return value;
}

// Steps down a power if value is already a power of 2.
inline constexpr uint32_t PreviousPowerOf2(uint32_t value)
{
	uint32_t result = 1;
	while (result * 2 < value)
		result *= 2;
	return result;
}