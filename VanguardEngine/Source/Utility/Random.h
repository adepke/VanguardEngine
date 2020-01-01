// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <random>

namespace Detail
{
	auto thread_local Generator = std::mt19937_64{};
}

void Seed(std::seed_seq InSeed)
{
	Detail::Generator.seed(InSeed);
}

int Rand(int Min, int Max)
{
	return std::uniform_int_distribution{ Min, Max }(Detail::Generator);
}

double Rand(double Min, double Max)
{
	return std::uniform_real_distribution{ Min, Max }(Detail::Generator);
}