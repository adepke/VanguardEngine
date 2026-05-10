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

// Cheap approximation of Henyey-Greenstein. Note that k can be precomputed for a given g.
// nu = cos(theta)
float SchlickPhase(float nu, float g)
{
	const float k = 1.55 * g - 0.55 * g*g*g;
	const float numerator = 1.0 - k*k;
	const float bottomTerm = 1.0 + k * nu;
	const float denominator = bottomTerm*bottomTerm * 4.0 * pi;
	return numerator / denominator;
}

// Generalization of Henyey-Greenstein with an additional cos^2 term that
// improves the back-scatter shape for larger spherical scatterers.
float DrainePhase(float nu, float g, float alpha)
{
	const float g2 = g * g;
	const float numerator = (1.0 - g2) * (1.0 + alpha * nu * nu);
	const float bottomTerm = 1.0 + g2 - 2.0 * g * nu;
	const float denominator = pow(max(bottomTerm, 1e-6), 1.5) * (1.0 + alpha * (1.0 + 2.0 * g2) / 3.0) * 4.0 * pi;
	return numerator / denominator;
}

// Jendersie & d'Eon (2023) approximate Mie scattering for fog/cloud rendering.
// "An Approximate Mie Scattering Function for Fog and Cloud Rendering", SIGGRAPH 2023.
// See: https://research.nvidia.com/labs/rtr/approximate-mie/
// Models a polydisperse population of water droplets at a representative effective diameter `d`
// (in microns). The fit is published for 5 <= d <= 50 microns. For typical cumulus cloud droplets,
// d ~= 5-15 um; cloud silver-lining d ~= 20-50 um.
// The phase is a wD-weighted blend of an HG forward peak and a Draine lobe (capturing the bulk
// shape) and matches >=95% of the energy of a tabulated Mie reference in the forward hemisphere.
float JendersieDEonPhase(float nu, float dropletDiameter)
{
	const float d = clamp(dropletDiameter, 5.0, 50.0);  // Authors' fit is valid for d in [5, 50] um.

	// Forward HG lobe: strong, narrow forward peak.
	const float gHG = exp(-0.0990567 / (d - 1.67154));
	// Draine lobe: bulk shape (forward asymmetry plus the cos^2 angular term gives most of the
	// off-peak distribution).
	const float gD = exp(-2.20679 / (d + 3.91029)) - 0.428934;
	// cos^2 angular asymmetry of the Draine lobe.
	const float alpha = exp(3.62489 - 8.29288 / (d + 5.52825));
	// Weight of the Draine lobe vs the HG forward peak.
	const float wD = exp(-0.599085 / (d - 0.641583)) - 0.665888;

	const float hg = HenyeyGreensteinPhase(nu, gHG);
	const float draine = DrainePhase(nu, gD, alpha);
	return (1.0 - wD) * hg + wD * draine;
}

#endif  // __PHASEFUNCTIONS_HLSLI__