// Copyright (c) 2019-2021 Andrew Depke

#include <vector>
#include <cstdint>
#include <cmath>

// Generates 1D Gaussian filter weights, from center to edge.
// https://en.wikipedia.org/wiki/Gaussian_filter
std::vector<float> GaussianKernel(uint32_t radius, float sigma)
{
	std::vector<float> result;
	result.resize(radius, 0.f);

	float sum = 0.f;
	for (int i = 0; i < radius; ++i)
	{
		result[i] = std::exp(-std::pow(i, 2.f) / (2.f * std::pow(sigma, 2.f)));
		sum += result[i];
	}

	sum *= 2.f;  // Mirror.
	sum -= result[0];  // Remove the duplicated center.

	// Normalize.
	float alpha = 1.f / sum;
	for (int i = 0; i < radius; ++i)
	{
		result[i] *= alpha;
	}

	return result;
}