// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <random>

namespace Detail
{
	inline auto thread_local generator = std::mt19937_64{};
}

inline void Seed(std::seed_seq seed)
{
	Detail::generator.seed(seed);
}

inline int Rand(int min, int max)
{
	return std::uniform_int_distribution{ min, max }(Detail::generator);
}

inline double Rand(double min, double max)
{
	return std::uniform_real_distribution{ min, max }(Detail::generator);
}