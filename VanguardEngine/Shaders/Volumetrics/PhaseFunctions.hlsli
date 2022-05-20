// Copyright (c) 2019-2022 Andrew Depke

#ifndef __PHASEFUNCTIONS_HLSLI__
#define __PHASEFUNCTIONS_HLSLI__

#include "Constants.hlsli"

// Uniform scattering.
float IsotropicPhase(float theta)
{
	return 1.0 / (4.0 * pi);
}

// Small particle scattering.
// nu = cos(theta)
float RayleighPhase(float nu)
{
	static const float k = 3.f / (16.f * pi);
	return k * (1.f + nu * nu);
}

// Large particle scattering.
// nu = cos(theta), g = anisotropy (0=isotropic)
float MiePhase(float nu, float g)
{
	const float gSquared = g * g;
	const float k = 3.f / (8.f * pi) * (1.f - gSquared) / (2.f + gSquared);
	return k * (1.f + nu * nu) / pow(1.f + gSquared - 2.f * g * nu, 1.5f);
}

// Large particle scattering, cheaper than mie.
// nu = cos(theta)
float HenyeyGreensteinPhase(float nu, float eccentricity)
{
	const float g2 = eccentricity * eccentricity;
	const float numerator = 1.0 - g2;
	const float denominator = pow(1.0 + g2 - 2.0 * eccentricity * nu, 1.5) * 4.0 * pi;
	return numerator / denominator;
}

#endif  // __PHASEFUNCTIONS_HLSLI__