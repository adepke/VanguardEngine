// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <functional>

// Taken from: https://stackoverflow.com/a/54728293
template <typename T, typename... R>
void HashCombine(size_t& seed, const T& value, R... rest)
{
	seed ^= std::hash<std::decay_t<T>>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	(HashCombine(seed, rest), ...);
}