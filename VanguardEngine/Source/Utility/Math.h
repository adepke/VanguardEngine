// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <cstdint>

// Taken from: https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
constexpr uint32_t NextPowerOf2(uint32_t value)
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