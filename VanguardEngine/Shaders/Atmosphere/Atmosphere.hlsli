// Copyright (c) 2019-2022 Andrew Depke

#ifndef __ATMOSPHERE_HLSLI__
#define __ATMOSPHERE_HLSLI__

#include "Constants.hlsli"
#include "Camera.hlsli"
#include "Volumetrics/PhaseFunctions.hlsli"

// Atmospheric scattering, inspired by "Precomputed Atmospheric Rendering" [https://hal.inria.fr/inria-00288758/en]
// and with additions from Frostbite [https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf]
// Then, some additional modifications for this project:
//   1. GetSkyRadiance applies a horizon bias on mu. This is to fix the horizon LUT disparity for mountains.
//   2. Both GetSkyRadiance and GetSkyRadianceToPoint apply the bias symmetrically (reverse bias on
//      ground-intersecting rays). Bruneton only biases sky-side rays.
//   3. GetSkyRadianceToPoint takes an explicit `endpointIsGround` parameter instead of computing
//      rayIntersectsGround from the idealized camera-ray vs planet-sphere check. Required to handle
//      object hits (terrain meshes, clouds) that obstruct the ray before it would reach the planet
//      sphere. Without it, mountains cross the horizon line sample two different LUT halves and
//      show a nasty visible break. Ref: ebruneton/precomputed_atmospheric_scattering#26.
//   4. The scalar `shadowLength` is replaced by a `(shadowStart, shadowLength)` segment that places the
//      shadow at an arbitrary location along the ray. Both atmosphere functions use the same shadow
//      representation, fixing bad behavior with the Bruneton's end-of-ray convention. Centroid is
//      computed in Clouds/Visibility.hlsl from shadow moments.
//      #TODO: Replace the scalar shadow with clustered (froxel) volumetrics. Ideally split into
//      a near, fine cluster for ground-level fog/smoke/dust and a far, coarse cluster for clouds and
//      other large media. This will improve shadow distribution and remove the single-segment hack
//      entirely.

// Density = exponentialCoefficient * exp(exponentialScale * height) + heightScale * height + offset
struct DensityLayer
{
	float width;
	float exponentialCoefficient;
	float exponentialScale;
	float heightScale;
	// Boundary
	float offset;
	float3 padding;
};

struct AtmosphereData
{
	float radiusBottom;  // Planet center to the start of the atmosphere.
	float radiusTop;
	float2 padding0;
	
	DensityLayer rayleighDensity;
	float3 rayleighScattering;  // Air molecule scattering, absorption is considered negligible.
	float padding1;
	
	DensityLayer mieDensity;
	float3 mieScattering;
	float padding2;
	float3 mieExtinction;
	float padding3;
	
	DensityLayer absorptionDensity;
	float3 absorptionExtinction;
	float padding4;
	
	float3 surfaceColor;  // Average albedo of the planet surface.
	float padding5;
	
	float3 solarIrradiance;  // #TODO: Separate sun data out of the atmosphere.
	float padding6;
};

static const float sunAngularRadius = 0.004675f;
static const float minMuS = -0.5f;
static const float mieAnisotropy = 0.8f;

float DistanceToAtmosphereTop(AtmosphereData atmosphere, float radius, float mu)
{
	float discriminant = radius * radius * (mu * mu - 1.f) + atmosphere.radiusTop * atmosphere.radiusTop;
	return max(-radius * mu + sqrt(max(discriminant, 0.f)), 0.f);
}

float DistanceToAtmosphereBottom(AtmosphereData atmosphere, float radius, float mu)
{
	float discriminant = radius * radius * (mu * mu - 1.f) + atmosphere.radiusBottom * atmosphere.radiusBottom;
	return max(-radius * mu - sqrt(max(discriminant, 0.f)), 0.f);
}

float DistanceToNearestAtmosphereEdge(AtmosphereData atmosphere, float radius, float mu, bool rayIntersectsGround)
{
	if (rayIntersectsGround)
	{
		return DistanceToAtmosphereBottom(atmosphere, radius, mu);
	}
	
	else
	{
		return DistanceToAtmosphereTop(atmosphere, radius, mu);
	}
}

bool RayIntersectsGround(AtmosphereData atmosphere, float radius, float mu)
{
	return mu < 0.f && radius * radius * (mu * mu - 1.f) + atmosphere.radiusBottom * atmosphere.radiusBottom >= 0.f;
}

float GetAtmosphereLayerDensity(DensityLayer layer, float height)
{
	return saturate(layer.exponentialCoefficient * exp(layer.exponentialScale * height) + layer.heightScale * height + layer.offset);
}

float ComputeOpticalLengthToAtmosphereTop(AtmosphereData atmosphere, DensityLayer layer, float radius, float mu)
{
	// Intervals for numerical integration.
	static const int steps = 500;
	
	float dx = DistanceToAtmosphereTop(atmosphere, radius, mu) / steps;
	float result = 0.f;
	
	for (int i = 0; i <= steps; ++i)
	{
		float d_i = i * dx;
		float r_i = sqrt(d_i * d_i + 2.f * radius * mu * d_i + radius * radius);  // Distance between sample point and planet center.
		float y_i = GetAtmosphereLayerDensity(layer, r_i - atmosphere.radiusBottom);
		float weight_i = (i == 0 || i == steps) ? 0.5f : 1.f;
		
		result += y_i * weight_i * dx;
	}
	
	return result;
}

float3 ComputeTransmittanceToAtmosphereTop(AtmosphereData atmosphere, float radius, float mu)
{
	// No rayleigh absorption, so extinction = scattering.
	float3 rayleigh = atmosphere.rayleighScattering * ComputeOpticalLengthToAtmosphereTop(atmosphere, atmosphere.rayleighDensity, radius, mu);
	float3 mie = atmosphere.mieExtinction * ComputeOpticalLengthToAtmosphereTop(atmosphere, atmosphere.mieDensity, radius, mu);
	float3 absorption = atmosphere.absorptionExtinction * ComputeOpticalLengthToAtmosphereTop(atmosphere, atmosphere.absorptionDensity, radius, mu);
	
	return exp(-(rayleigh + mie + absorption));
}

float UnitRangeToTextureCoord(float x, int textureSize)
{
	return 0.5f / textureSize + x * (1.f - 1.f / textureSize);
}

float TextureCoordToUnitRange(float u, int textureSize)
{
	return (u - 0.5f / textureSize) / (1.f - 1.f / textureSize);
}

float2 GetTransmittanceLutCoord(AtmosphereData atmosphere, float radius, float mu, float2 textureSize)
{
	float atmosphereBottomRadiusSquared = atmosphere.radiusBottom * atmosphere.radiusBottom;
	
	float h = sqrt(atmosphere.radiusTop * atmosphere.radiusTop - atmosphereBottomRadiusSquared);
	float rho = sqrt(max(radius * radius - atmosphereBottomRadiusSquared, 0.f));
	float d = DistanceToAtmosphereTop(atmosphere, radius, mu);
	float dMin = atmosphere.radiusTop - radius;
	float dMax = h + rho;
	float xMu = (d - dMin) / (dMax - dMin);
	float xRadius = rho / h;
	
	return float2(UnitRangeToTextureCoord(xMu, textureSize.x), UnitRangeToTextureCoord(xRadius, textureSize.y));
}

float2 GetTransmittanceLutData(AtmosphereData atmosphere, float2 uv, float2 textureSize)
{
	float xMu = TextureCoordToUnitRange(uv.x, textureSize.x);
	float xRadius = TextureCoordToUnitRange(uv.y, textureSize.y);
	
	float atmosphereOuterRadiusSquared = atmosphere.radiusTop * atmosphere.radiusTop;
	float atmosphereRadiusSquared = atmosphere.radiusBottom * atmosphere.radiusBottom;
	
	float h = sqrt(atmosphere.radiusTop * atmosphere.radiusTop - atmosphereRadiusSquared);
	float rho = h * xRadius;
	float radius = sqrt(rho * rho + atmosphereRadiusSquared);
	float dMin = atmosphere.radiusTop - radius;
	float dMax = h + rho;
	float d = dMin + xMu * (dMax - dMin);
	float mu = d == 0.f ? 1.f : clamp((h * h - rho * rho - d * d) / (2.f * radius * d), -1.f, 1.f);
	
	return float2(radius, mu);
}

float3 ComputeTransmittanceToAtmosphereTopLut(AtmosphereData atmosphere, float2 uv, float2 textureSize)
{
	float2 radiusMu = GetTransmittanceLutData(atmosphere, uv, textureSize);
	
	return ComputeTransmittanceToAtmosphereTop(atmosphere, radiusMu.x, radiusMu.y);
}

float3 GetTransmittanceToAtmosphereTop(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float radius, float mu)
{
	float2 textureSize;
	transmittanceLut.GetDimensions(textureSize.x, textureSize.y);
	float2 uv = GetTransmittanceLutCoord(atmosphere, radius, mu, textureSize);
	
	return transmittanceLut.SampleLevel(lutSampler, uv, 0).xyz;
}

float3 GetTransmittance(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float radius, float mu, float d, bool rayIntersectsGround)
{
	float minTransmittance = 1.f;
	float radius_d = clamp(sqrt(d * d + 2.f * radius * mu * d + radius * radius), atmosphere.radiusBottom, atmosphere.radiusTop);
	float mu_d = clamp((radius * mu + d) / radius_d, -1.f, 1.f);
	
	if (rayIntersectsGround)
	{
		return min(
			GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius_d, -mu_d) / GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius, -mu),
			minTransmittance.xxx
		);
	}
	
	else
	{
		return min(
			GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius, mu) / GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius_d, mu_d),
			minTransmittance.xxx
		);
	}
}

float3 GetTransmittanceToSun(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float radius, float muS)
{
	float sinThetaH = atmosphere.radiusBottom / radius;
	float cosThetaH = -sqrt(max(1.f - sinThetaH * sinThetaH, 0.f));
	
	return GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius, muS) * smoothstep(-sinThetaH * sunAngularRadius, sinThetaH * sunAngularRadius, muS - cosThetaH);
}

void ComputeSingleScatteringIntegrand(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler,
	float radius, float mu, float muS, float nu, float d, bool rayIntersectsGround, out float3 rayleigh, out float3 mie)
{
	float radius_d = clamp(sqrt(d * d + 2.f * radius * mu * d + radius * radius), atmosphere.radiusBottom, atmosphere.radiusTop);
	float muS_d = clamp((radius * muS + d * nu) / radius_d, -1.f, 1.f);
	float3 transmittance = GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, mu, d, rayIntersectsGround) * GetTransmittanceToSun(atmosphere, transmittanceLut, lutSampler, radius_d, muS_d);
	
	rayleigh = transmittance * GetAtmosphereLayerDensity(atmosphere.rayleighDensity, radius_d - atmosphere.radiusBottom);
	mie = transmittance * GetAtmosphereLayerDensity(atmosphere.mieDensity, radius_d - atmosphere.radiusBottom);
}

void ComputeSingleScattering(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler,
	float radius, float mu, float muS, float nu, bool rayIntersectsGround, out float3 rayleigh, out float3 mie)
{
	// Intervals for numerical integration.
	static const int steps = 50;
	
	float dx = DistanceToNearestAtmosphereEdge(atmosphere, radius, mu, rayIntersectsGround) / steps;
	float3 rayleighSum = 0.f;
	float3 mieSum = 0.f;
	
	for (int i = 0; i <= steps; ++i)
	{
		float d_i = i * dx;
		float3 rayleigh_i;
		float3 mie_i;
		ComputeSingleScatteringIntegrand(atmosphere, transmittanceLut, lutSampler, radius, mu, muS, nu, d_i, rayIntersectsGround, rayleigh_i, mie_i);
		float weight_i = (i == 0 || i == steps) ? 0.5f : 1.f;
		
		rayleighSum += rayleigh_i * weight_i;
		mieSum += mie_i * weight_i;
	}
	
	rayleigh = rayleighSum * dx * atmosphere.solarIrradiance * atmosphere.rayleighScattering;
	mie = mieSum * dx * atmosphere.solarIrradiance * atmosphere.mieScattering;
}

float4 GetScatteringLutDimensions4D(float3 textureSize)
{
	static const float nuSize = 8.f;
	
	// Radius, mu, muS, nu.
	return float4(textureSize.z, textureSize.y, textureSize.x / nuSize, nuSize);
}

float4 GetScatteringLutCoord(AtmosphereData atmosphere, float radius, float mu, float muS, float nu, bool rayIntersectsGround, float4 textureSize)
{
	float atmosphereRadiusSquared = atmosphere.radiusBottom * atmosphere.radiusBottom;
	float radiusSquared = radius * radius;
	
	float h = sqrt(atmosphere.radiusTop * atmosphere.radiusTop - atmosphereRadiusSquared);
	float rho = sqrt(max(radiusSquared - atmosphereRadiusSquared, 0.f));
	float uRadius = UnitRangeToTextureCoord(rho / h, textureSize.x);
	float radiusMu = radius * mu;
	float discriminant = radiusMu * radiusMu - radiusSquared + atmosphereRadiusSquared;
	
	float uMu;
	if (rayIntersectsGround)
	{
		float d = -radiusMu - sqrt(max(discriminant, 0.f));
		float dMin = radius - atmosphere.radiusBottom;
		float dMax = rho;
		uMu = 0.5f - 0.5f * UnitRangeToTextureCoord(dMax == dMin ? 0.f : (d - dMin) / (dMax - dMin), textureSize.y / 2.f);
	}
	
	else
	{
		float d = -radiusMu + sqrt(max(discriminant + h * h, 0.f));
		float dMin = atmosphere.radiusTop - radius;
		float dMax = h + rho;
		uMu = 0.5f + 0.5f * UnitRangeToTextureCoord((d - dMin) / (dMax - dMin), textureSize.y / 2.f);
	}
	
	float d = DistanceToAtmosphereTop(atmosphere, atmosphere.radiusBottom, muS);
	float dMin = atmosphere.radiusTop - atmosphere.radiusBottom;
	float dMax = h;
	float a = (d - dMin) / (dMax - dMin);
	float D = DistanceToAtmosphereTop(atmosphere, atmosphere.radiusBottom, minMuS);
	float A = (D - dMin) / (dMax - dMin);
	float uMuS = UnitRangeToTextureCoord(max(1.f - a / A, 0.f) / (1.f + a), textureSize.z);
	float uNu = (nu + 1.f) / 2.f;
	
	return float4(uNu, uMuS, uMu, uRadius);
}

struct ScatteringLutData
{
	float radius;
	float mu;
	float muS;
	float nu;
	bool rayIntersectsGround;
};

ScatteringLutData GetScatteringLutData(AtmosphereData atmosphere, float4 uvwz, float4 textureSize)
{
	float atmosphereRadiusSquared = atmosphere.radiusBottom * atmosphere.radiusBottom;
	
	float h = sqrt(atmosphere.radiusTop * atmosphere.radiusTop - atmosphereRadiusSquared);
	float rho = h * TextureCoordToUnitRange(uvwz.w, textureSize.x);
	
	ScatteringLutData lutData;
	lutData.radius = sqrt(rho * rho + atmosphereRadiusSquared);
	
	if (uvwz.z < 0.5f)
	{
		float dMin = lutData.radius - atmosphere.radiusBottom;
		float dMax = rho;
		float d = dMin + (dMax - dMin) * TextureCoordToUnitRange(1.f - 2.f * uvwz.z, textureSize.y / 2.f);
		lutData.mu = d == 0.f ? -1.f : clamp(-(rho * rho + d * d) / (2.f * lutData.radius * d), -1.f, 1.f);
		lutData.rayIntersectsGround = true;
	}
	
	else
	{
		float dMin = atmosphere.radiusTop - lutData.radius;
		float dMax = h + rho;
		float d = dMin + (dMax - dMin) * TextureCoordToUnitRange(2.f * uvwz.z - 1.f, textureSize.y / 2.f);
		lutData.mu = d == 0.f ? 1.f : clamp((h * h - rho * rho - d * d) / (2.f * lutData.radius * d), -1.f, 1.f);
		lutData.rayIntersectsGround = false;
	}
	
	float xMuS = TextureCoordToUnitRange(uvwz.y, textureSize.z);
	float dMin = atmosphere.radiusTop - atmosphere.radiusBottom;
	float dMax = h;
	float D = DistanceToAtmosphereTop(atmosphere, atmosphere.radiusBottom, minMuS);
	float A = (D - dMin) / (dMax - dMin);
	float a = (A - xMuS * A) / (1.f + xMuS * A);
	float d = dMin + min(a, A) * (dMax - dMin);
	lutData.muS = d == 0.f ? 1.f : clamp((h * h - d * d) / (2.f * atmosphere.radiusBottom * d), -1.f, 1.f);
	lutData.nu = clamp(uvwz.x * 2.f - 1.f, -1.f, 1.f);
	
	return lutData;
}

ScatteringLutData GetScatteringLutData(AtmosphereData atmosphere, float3 uvw, float4 textureSize)
{
	float nuCoord = floor(uvw.x / textureSize.z);
	float muSCoord = fmod(uvw.x, textureSize.z);
	float4 uvwz = float4(nuCoord, muSCoord, uvw.y, uvw.z) / float4(textureSize.w - 1.f, textureSize.z, textureSize.y, textureSize.x);
	
	ScatteringLutData lutData = GetScatteringLutData(atmosphere, uvwz, textureSize);
	float muSquared = lutData.mu * lutData.mu;
	float muSSquared = lutData.muS * lutData.muS;
	lutData.nu = clamp(lutData.nu, lutData.mu * lutData.muS - sqrt((1.f - muSquared) * (1.f - muSSquared)), lutData.mu * lutData.muS + sqrt((1.f - muSquared) * (1.f - muSSquared)));
	
	return lutData;
}

void ComputeSingleScatteringLut(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float3 uvw, float3 textureSize, out float3 rayleigh, out float3 mie)
{
	ScatteringLutData lutData = GetScatteringLutData(atmosphere, uvw * textureSize, GetScatteringLutDimensions4D(textureSize));
	ComputeSingleScattering(atmosphere, transmittanceLut, lutSampler, lutData.radius, lutData.mu, lutData.muS, lutData.nu, lutData.rayIntersectsGround, rayleigh, mie);
}

float3 GetScattering(AtmosphereData atmosphere, Texture3D scatteringLut, SamplerState lutSampler, float radius, float mu, float muS, float nu, bool rayIntersectsGround)
{
	float3 dimensions;
	scatteringLut.GetDimensions(dimensions.x, dimensions.y, dimensions.z);
	float4 textureSize = GetScatteringLutDimensions4D(dimensions);
	
	float4 uvwz = GetScatteringLutCoord(atmosphere, radius, mu, muS, nu, rayIntersectsGround, textureSize);
	float lutCoordX = uvwz.x * (textureSize.w - 1.f);
	float lutX = floor(lutCoordX);
	float lerp = lutCoordX - lutX;
	float3 uvw0 = float3((lutX + uvwz.y) / textureSize.w, uvwz.z, uvwz.w);
	float3 uvw1 = float3((lutX + 1.f + uvwz.y) / textureSize.w, uvwz.z, uvwz.w);
	
	return scatteringLut.SampleLevel(lutSampler, uvw0, 0).xyz * (1.f - lerp) + scatteringLut.SampleLevel(lutSampler, uvw1, 0).xyz * lerp;
}

float3 GetScattering(AtmosphereData atmosphere, Texture3D singleRayleighScatteringLut, Texture3D singleMieScatteringLut, Texture3D multipleScatteringLut, SamplerState lutSampler,
	float radius, float mu, float muS, float nu, bool rayIntersectsGround, int scatteringOrder)
{
	if (scatteringOrder == 1)
	{
		float3 rayleigh = GetScattering(atmosphere, singleRayleighScatteringLut, lutSampler, radius, mu, muS, nu, rayIntersectsGround);
		float3 mie = GetScattering(atmosphere, singleMieScatteringLut, lutSampler, radius, mu, muS, nu, rayIntersectsGround);
		
		return rayleigh * RayleighPhase(nu) + mie * MiePhase(nu, mieAnisotropy);
	}
	
	else
	{
		return GetScattering(atmosphere, multipleScatteringLut, lutSampler, radius, mu, muS, nu, rayIntersectsGround);
	}
}

float3 ComputeDirectIrradiance(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float radius, float muS)
{
	float alphaS = sunAngularRadius;
	float avgCosFactor = muS < -alphaS ? 0.f : (muS > alphaS ? muS : (muS + alphaS) * (muS + alphaS) / (4.f * alphaS));

	return atmosphere.solarIrradiance * GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius, muS) * avgCosFactor;
}

float3 ComputeIndirectIrradiance(AtmosphereData atmosphere, Texture3D singleRayleighScatteringLut, Texture3D singleMieScatteringLut, Texture3D multipleScatteringLut, SamplerState lutSampler,
	float radius, float muS, int scatteringOrder)
{
	// Intervals for numerical integration.
	static const int steps = 32;
	
	float dTheta = pi / steps;
	float dPhi = pi / steps;
	
	float3 result = 0.f;
	float3 omegaS = float3(sqrt(1.f - muS * muS), 0.f, muS);
	
	for (int i = 0; i < steps / 2; ++i)
	{
		float theta = (i + 0.5f) * dTheta;

		for (int j = 0; j < steps * 2; ++j)
		{
			float phi = (j + 0.5f) * dPhi;
			float3 omega = float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
			float dOmega = dTheta * dPhi * sin(theta);
			float nu = dot(omega, omegaS);
			
			result += GetScattering(atmosphere, singleRayleighScatteringLut, singleMieScatteringLut, multipleScatteringLut, lutSampler, radius, omega.z, muS, nu, false, scatteringOrder) * omega.z * dOmega;
		}
	}
	
	return result;
}

float2 GetIrradianceLutCoord(AtmosphereData atmosphere, float radius, float muS, float2 textureSize)
{
	float xRadius = (radius - atmosphere.radiusBottom) / (atmosphere.radiusTop - atmosphere.radiusBottom);
	float xMuS = muS * 0.5f + 0.5f;
	
	return float2(UnitRangeToTextureCoord(xMuS, textureSize.x), UnitRangeToTextureCoord(xRadius, textureSize.y));
}

float2 GetIrradianceLutData(AtmosphereData atmosphere, float2 uv, float2 textureSize)
{
	float xMuS = TextureCoordToUnitRange(uv.x, textureSize.x);
	float xRadius = TextureCoordToUnitRange(uv.y, textureSize.y);
	
	return float2(atmosphere.radiusBottom + xRadius * (atmosphere.radiusTop - atmosphere.radiusBottom), clamp(2.f * xMuS - 1.f, -1.f, 1.f));
}

float3 ComputeDirectIrradianceLut(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float2 uv, float2 textureSize)
{
	float2 lutData = GetIrradianceLutData(atmosphere, uv, textureSize);
	
	return ComputeDirectIrradiance(atmosphere, transmittanceLut, lutSampler, lutData.x, lutData.y);
}

float3 ComputeIndirectIrradianceLut(AtmosphereData atmosphere, Texture3D singleRayleighScatteringLut, Texture3D singleMieScatteringLut, Texture3D multipleScatteringLut, SamplerState lutSampler,
	float2 uv, int scatteringOrder, float2 textureSize)
{
	float2 lutData = GetIrradianceLutData(atmosphere, uv, textureSize);
	
	return ComputeIndirectIrradiance(atmosphere, singleRayleighScatteringLut, singleMieScatteringLut, multipleScatteringLut, lutSampler, lutData.x, lutData.y, scatteringOrder);
}

float3 GetIrradiance(AtmosphereData atmosphere, Texture2D irradianceLut, SamplerState lutSampler, float radius, float muS)
{
	float2 textureSize;
	irradianceLut.GetDimensions(textureSize.x, textureSize.y);
	float2 uv = GetIrradianceLutCoord(atmosphere, radius, muS, textureSize);
	
	return irradianceLut.SampleLevel(lutSampler, uv, 0).xyz;
}

float3 ComputeScatteringDensity(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture3D singleRayleighScatteringLut, Texture3D singleMieScatteringLut, Texture3D multipleScatteringLut, Texture2D irradianceLut, SamplerState lutSampler,
	float radius, float mu, float muS, float nu, int scatteringOrder)
{
	float3 zenithDirection = float3(0.f, 0.f, 1.f);
	float3 omega = float3(sqrt(1.f - mu * mu), 0.f, mu);
	float sunDirectionX = omega.x == 0.f ? 0.f : (nu - mu * muS) / omega.x;
	float sunDirectionY = sqrt(max(1.f - sunDirectionX * sunDirectionX - muS * muS, 0.f));
	float3 omegaS = float3(sunDirectionX, sunDirectionY, muS);
	
	// Intervals for numerical integration.
	static const int steps = 16;
	
	float dTheta = pi / steps;
	float dPhi = pi / steps;
	float3 rayleighMie = 0.f;
	
	for (int i = 0; i < steps; ++i)
	{
		float theta = (i + 0.5f) * dTheta;
		float cosTheta = cos(theta);
		float sinTheta = sin(theta);
		bool rayIntersectsGround = RayIntersectsGround(atmosphere, radius, cosTheta);
		
		float distanceToGround = 0.f;
		float3 transmittanceToGround = 0.f;
		float3 groundAlbedo = 0.f;
		
		if (rayIntersectsGround)
		{
			distanceToGround = DistanceToAtmosphereBottom(atmosphere, radius, cosTheta);
			transmittanceToGround = GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, cosTheta, distanceToGround, true);
			groundAlbedo = atmosphere.surfaceColor;
		}
		
		for (int j = 0; j < steps * 2; ++j)
		{
			float phi = (j + 0.5f) * dPhi;
			float3 omegaI = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
			float dOmegaI = dTheta * dPhi * sin(theta);
			
			float nu1 = dot(omegaS, omegaI);
			float3 incidentRadiance = GetScattering(atmosphere, singleRayleighScatteringLut, singleMieScatteringLut, multipleScatteringLut, lutSampler, radius, omegaI.z, muS, nu1, rayIntersectsGround, scatteringOrder - 1);
			
			float3 groundNormal = normalize(zenithDirection * radius + omegaI * distanceToGround);
			float3 groundIrradiance = GetIrradiance(atmosphere, irradianceLut, lutSampler, atmosphere.radiusBottom, dot(groundNormal, omegaS));
			incidentRadiance += transmittanceToGround * groundAlbedo * groundIrradiance / pi;
			
			float nu2 = dot(omega, omegaI);
			float rayleighDensity = GetAtmosphereLayerDensity(atmosphere.rayleighDensity, radius - atmosphere.radiusBottom);
			float mieDensity = GetAtmosphereLayerDensity(atmosphere.mieDensity, radius - atmosphere.radiusBottom);
			
			rayleighMie += incidentRadiance * (atmosphere.rayleighScattering * rayleighDensity * RayleighPhase(nu2) + atmosphere.mieScattering * mieDensity * MiePhase(nu2, mieAnisotropy)) * dOmegaI;
		}
	}
	
	return rayleighMie;
}

float3 ComputeMultipleScattering(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture3D scatteringDensityLut, SamplerState lutSampler,
	float radius, float mu, float muS, float nu, bool rayIntersectsGround)
{
	// Intervals for numerical integration.
	static const int steps = 50;
	
	float dx = DistanceToNearestAtmosphereEdge(atmosphere, radius, mu, rayIntersectsGround) / steps;
	float3 sum = 0.f;
	
	for (int i = 0; i <= steps; ++i)
	{
		float dI = i * dx;
		float radiusI = clamp(sqrt(dI * dI + 2.f * radius * mu * dI + radius * radius), atmosphere.radiusBottom, atmosphere.radiusTop);
		float muI = clamp((radius * mu + dI) / radiusI, -1.f, 1.f);
		float muSI = clamp((radius * muS + dI * nu) / radiusI, -1.f, 1.f);
		
		float3 rayleighMieI = GetScattering(atmosphere, scatteringDensityLut, lutSampler, radiusI, muI, muSI, nu, rayIntersectsGround) * GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, mu, dI, rayIntersectsGround) * dx;
		float weightI = (i == 0 || i == steps) ? 0.5f : 1.f;
		
		sum += rayleighMieI * weightI;
	}
	
	return sum;
}

float3 ComputeScatteringDensityLut(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture3D singleRayleighScatteringLut, Texture3D singleMieScatteringLut, Texture3D multipleScatteringLut, Texture2D irradianceLut, SamplerState lutSampler,
	float3 uvw, int scatteringOrder)
{
	//float3 textureSize;
	// #TEMP
	float3 textureSize = float3(0.f, 0.f, 0.f);
	multipleScatteringLut.GetDimensions(textureSize.x, textureSize.y, textureSize.z);
	ScatteringLutData lutData = GetScatteringLutData(atmosphere, uvw * textureSize, GetScatteringLutDimensions4D(textureSize));
	
	return ComputeScatteringDensity(atmosphere, transmittanceLut, singleRayleighScatteringLut, singleMieScatteringLut, multipleScatteringLut, irradianceLut, lutSampler, lutData.radius, lutData.mu, lutData.muS, lutData.nu, scatteringOrder);
}

float4 ComputeMultipleScatteringLut(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture3D scatteringDensityLut, SamplerState lutSampler, float3 uvw)
{
	//float3 textureSize;
	// #TEMP
	float3 textureSize = float3(0.f, 0.f, 0.f);
	scatteringDensityLut.GetDimensions(textureSize.x, textureSize.y, textureSize.z);
	ScatteringLutData lutData = GetScatteringLutData(atmosphere, uvw * textureSize, GetScatteringLutDimensions4D(textureSize));
	
	return float4(ComputeMultipleScattering(atmosphere, transmittanceLut, scatteringDensityLut, lutSampler, lutData.radius, lutData.mu, lutData.muS, lutData.nu, lutData.rayIntersectsGround), lutData.nu);
}

float3 GetExtrapolatedSingleMieScattering(AtmosphereData atmosphere, float4 scattering)
{
	if (scattering.r <= 0.f)
	{
		return 0.f;
	}
	
	return scattering.rgb * scattering.a / scattering.r * (atmosphere.rayleighScattering.r / atmosphere.mieScattering.r) * (atmosphere.mieScattering / atmosphere.rayleighScattering);
}

float3 GetCombinedScattering(AtmosphereData atmosphere, Texture3D scatteringLut, SamplerState lutSampler,
	float radius, float mu, float muS, float nu, bool rayIntersectsGround, out float3 singleMieScattering)
{
	float3 dimensions;
	scatteringLut.GetDimensions(dimensions.x, dimensions.y, dimensions.z);
	float4 textureSize = GetScatteringLutDimensions4D(dimensions);
	float4 uvwz = GetScatteringLutCoord(atmosphere, radius, mu, muS, nu, rayIntersectsGround, textureSize);
	float lutCoordX = uvwz.x * (textureSize.w - 1.f);
	float lutX = floor(lutCoordX);
	float lerp = lutCoordX - lutX;
	
	float3 uvw0 = float3((lutX + uvwz.y) / textureSize.w, uvwz.z, uvwz.w);
	float3 uvw1 = float3((lutX + 1.f + uvwz.y) / textureSize.w, uvwz.z, uvwz.w);
	
	float4 combined = scatteringLut.SampleLevel(lutSampler, uvw0, 0) * (1.f - lerp) + scatteringLut.SampleLevel(lutSampler, uvw1, 0) * lerp;
	
	singleMieScattering = GetExtrapolatedSingleMieScattering(atmosphere, combined);
	return combined.xyz;
}

// Note Bruneton's model only takes a shadowLength and assumes the shadow is a contiguous block at
// the camera end of the ray (in GetSkyRadiance) OR at the far end (in GetSkyRadianceToPoint).
// This works for terrain and but has poor behavior with large shadow casting media like clouds.
// Adding a shadowStart term here helps mitigate this, but is still an approximation - eventually
// move to clustered volumetrics for a more correct general solution.
//
// Math (linearity of the LUT subtraction trick, generalized to a middle-of-ray segment):
//   S_visible(x->inf) = S(x->inf) − T(x->s_start) x S(s_start->inf) + T(x->s_end) x S(s_end->inf)
//   S_visible(x->y) = S_visible(x->inf) − T(x->y) x S(y->inf)
// Reduces to the standard Bruneton end-of-ray cases when shadowStart is 0 (sky-side shadow) or
// shadowStart = d − shadowLength (far-side shadow).
float3 GetSkyRadiance(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture3D scatteringLut, SamplerState lutSampler,
	float3 cameraPosition, float3 viewDirection, float shadowStart, float shadowLength, float3 sunDirection, out float3 transmittance)
{
	float radius = length(cameraPosition);
	float rMu = dot(cameraPosition, viewDirection);
	float distanceToAtmosphereTop = -rMu - sqrt(rMu * rMu - radius * radius + atmosphere.radiusTop * atmosphere.radiusTop);

	// If the camera is in space, move the position to be at the atmosphere top.
	if (distanceToAtmosphereTop > 0.f)
	{
		cameraPosition = cameraPosition + viewDirection * distanceToAtmosphereTop;
		radius = atmosphere.radiusTop;
		rMu += distanceToAtmosphereTop;
	}

	// View ray doesn't intersect the atmosphere.
	else if (radius > atmosphere.radiusTop)
	{
		transmittance = 1.f;
		return 0.f;
	}

	float mu = rMu / radius;
	float muS = dot(cameraPosition, sunDirection) / radius;
	float nu = dot(viewDirection, sunDirection);
	bool rayIntersectsGround = RayIntersectsGround(atmosphere, radius, mu);

	// Symmetric horizon bias. Reference applies no bias here at all, and applies only the sky-side bias inside
	// GetSkyRadianceToPoint. We bias both functions and both sides of the seam so the LUT — whose two
	// precomputed halves are independent and don't align across mu = muHorizon — is never sampled at the
	// discontinuity. Required for continuity where Compose flips between this and ToPoint.
	const float muHorizon = -sqrt(max(1.f - (atmosphere.radiusBottom / radius) * (atmosphere.radiusBottom / radius), 0.f));
	mu = rayIntersectsGround ? min(mu, muHorizon - 0.004f) : max(mu, muHorizon + 0.004f);

	transmittance = rayIntersectsGround ? 0.f : GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius, mu);

	// Base scattering: full integration from camera to the nearest atmosphere edge with no shadow.
	float3 singleMieScattering;
	float3 scattering = GetCombinedScattering(atmosphere, scatteringLut, lutSampler, radius, mu, muS, nu, rayIntersectsGround, singleMieScattering);

	// Shadow segment correction. Skipped entirely when shadowLength is zero so the unshadowed fast path costs the
	// same as before (1 scattering LUT + 1 transmittance LUT). With a shadow, this adds 2 scattering LUTs and
	// 2 transmittance LUTs.
	if (shadowLength > 0.f)
	{
		const float dStart = max(shadowStart, 0.f);
		const float dEnd   = shadowStart + shadowLength;

		// LUT lookup at s_start.
		const float radiusS = clamp(sqrt(dStart * dStart + 2.f * radius * mu * dStart + radius * radius), atmosphere.radiusBottom, atmosphere.radiusTop);
		const float muSc    = (radius * mu  + dStart)      / radiusS;
		const float muSSc   = (radius * muS + dStart * nu) / radiusS;
		float3 mieAtStart;
		const float3 scatAtStart  = GetCombinedScattering(atmosphere, scatteringLut, lutSampler, radiusS, muSc, muSSc, nu, rayIntersectsGround, mieAtStart);
		const float3 transToStart = GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, mu, dStart, rayIntersectsGround);

		// LUT lookup at s_end.
		const float radiusE = clamp(sqrt(dEnd * dEnd + 2.f * radius * mu * dEnd + radius * radius), atmosphere.radiusBottom, atmosphere.radiusTop);
		const float muEc    = (radius * mu  + dEnd)      / radiusE;
		const float muSEc   = (radius * muS + dEnd * nu) / radiusE;
		float3 mieAtEnd;
		const float3 scatAtEnd  = GetCombinedScattering(atmosphere, scatteringLut, lutSampler, radiusE, muEc, muSEc, nu, rayIntersectsGround, mieAtEnd);
		const float3 transToEnd = GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, mu, dEnd, rayIntersectsGround);

		scattering          = scattering          - transToStart * scatAtStart + transToEnd * scatAtEnd;
		singleMieScattering = singleMieScattering - transToStart * mieAtStart  + transToEnd * mieAtEnd;
		singleMieScattering = GetExtrapolatedSingleMieScattering(atmosphere, float4(scattering, singleMieScattering.r));
	}

	return scattering * RayleighPhase(nu) + singleMieScattering * MiePhase(nu, mieAnisotropy);
}

// See GetSkyRadiance shadowStart term.
float3 GetSkyRadianceToPoint(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture3D scatteringLut, SamplerState lutSampler,
	float3 cameraPosition, float3 position, bool endpointIsGround, float shadowStart, float shadowLength, float3 sunDirection, out float3 transmittance)
{
	float3 viewDirection = normalize(position - cameraPosition);
	float radius = length(cameraPosition);
	float radiusMu = dot(cameraPosition, viewDirection);
	float distanceToAtmosphereTop = -radiusMu - sqrt(radiusMu * radiusMu - radius * radius + atmosphere.radiusTop * atmosphere.radiusTop);

	if (distanceToAtmosphereTop > 0.f)
	{
		cameraPosition += viewDirection * distanceToAtmosphereTop;
		radius = atmosphere.radiusTop;
		radiusMu += distanceToAtmosphereTop;
	}

	float mu = radiusMu / radius;
	float muS = dot(cameraPosition, sunDirection) / radius;
	float nu = dot(viewDirection, sunDirection);
	float d = length(position - cameraPosition);
	bool rayIntersectsGround = endpointIsGround;

	// Symmetric horizon bias for LUT seam.
	const float muHorizon = -sqrt(max(1.f - (atmosphere.radiusBottom / radius) * (atmosphere.radiusBottom / radius), 0.f));
	mu = rayIntersectsGround ? min(mu, muHorizon - 0.004f) : max(mu, muHorizon + 0.004f);

	transmittance = GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, mu, d, rayIntersectsGround);

	// Having some pretty bad artifacts here, especially with strong mie scattering. Appears to be an issue with my implementation, but
	// I haven't determine the cause. Could be precomputation related, as my LUTs don't quite match Bruneton's. Solving this would require
	// an extensive analysis and debug, which I don't have time for now.

	// Base S(x->y) using Bruneton's standard subtraction trick: S(x->inf) − T(x->y) x S(y->∞).
	float3 singleMieScattering;
	float3 scattering = GetCombinedScattering(atmosphere, scatteringLut, lutSampler, radius, mu, muS, nu, rayIntersectsGround, singleMieScattering);

	const float radiusY = clamp(sqrt(d * d + 2.f * radius * mu * d + radius * radius), atmosphere.radiusBottom, atmosphere.radiusTop);
	const float muY     = (radius * mu  + d)      / radiusY;
	const float muSY    = (radius * muS + d * nu) / radiusY;
	float3 mieAtY;
	const float3 scatAtY = GetCombinedScattering(atmosphere, scatteringLut, lutSampler, radiusY, muY, muSY, nu, rayIntersectsGround, mieAtY);

	scattering          -= transmittance * scatAtY;
	singleMieScattering -= transmittance * mieAtY;

	// Apply the segment correction as: −T(x->s_start) x S(s_start->inf) + T(x->s_end) x S(s_end->inf)
	// which is the linearized form of "omit in-scattering across [s_start, s_end] of the camera-to-y path." The
	// shadow is clamped to lie within [0, d] so it never extends past the geometry endpoint.
	if (shadowLength > 0.f)
	{
		const float dStart = clamp(shadowStart, 0.f, d);
		const float dEnd   = clamp(shadowStart + shadowLength, dStart, d);

		// LUT lookup at s_start.
		const float radiusS = clamp(sqrt(dStart * dStart + 2.f * radius * mu * dStart + radius * radius), atmosphere.radiusBottom, atmosphere.radiusTop);
		const float muSc    = (radius * mu  + dStart)      / radiusS;
		const float muSSc   = (radius * muS + dStart * nu) / radiusS;
		float3 mieAtStart;
		const float3 scatAtStart  = GetCombinedScattering(atmosphere, scatteringLut, lutSampler, radiusS, muSc, muSSc, nu, rayIntersectsGround, mieAtStart);
		const float3 transToStart = GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, mu, dStart, rayIntersectsGround);

		// LUT lookup at s_end.
		const float radiusE = clamp(sqrt(dEnd * dEnd + 2.f * radius * mu * dEnd + radius * radius), atmosphere.radiusBottom, atmosphere.radiusTop);
		const float muEc    = (radius * mu  + dEnd)      / radiusE;
		const float muSEc   = (radius * muS + dEnd * nu) / radiusE;
		float3 mieAtEnd;
		const float3 scatAtEnd  = GetCombinedScattering(atmosphere, scatteringLut, lutSampler, radiusE, muEc, muSEc, nu, rayIntersectsGround, mieAtEnd);
		const float3 transToEnd = GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, mu, dEnd, rayIntersectsGround);

		scattering          = scattering          - transToStart * scatAtStart + transToEnd * scatAtEnd;
		singleMieScattering = singleMieScattering - transToStart * mieAtStart  + transToEnd * mieAtEnd;
	}

	singleMieScattering = GetExtrapolatedSingleMieScattering(atmosphere, float4(scattering, singleMieScattering.r));

	// Avoid rendering artifacts when the sun is below the horizon.
	singleMieScattering *= smoothstep(0.f, 0.01f, muS);

	return scattering * RayleighPhase(nu) + singleMieScattering * MiePhase(nu, mieAnisotropy);
}

void GetSunAndSkyIrradiance(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture2D irradianceLut, SamplerState lutSampler,
	float3 position, float3 normal, float3 sunDirection, out float3 sunIrradiance, out float3 skyIrradiance)
{
	float radius = length(position);
	float muS = dot(position, sunDirection) / radius;
	
	// Direct irradiance.
	sunIrradiance = atmosphere.solarIrradiance * GetTransmittanceToSun(atmosphere, transmittanceLut, lutSampler, radius, muS) * max(dot(normal, sunDirection), 0.f);
	// Indirect irradiance.
	skyIrradiance = GetIrradiance(atmosphere, irradianceLut, lutSampler, radius, muS) * (1.f + dot(normal, position) / radius) * 0.5f;
}

// The below separated variants of GetSunAndSkyIrradiance are used to decouple the atmosphere data from the geometry,
// which is necessary to render the atmopshere and atompshere-lit geometry in separate passes. Note that this separation
// is an approximation, since the position is still in the precalculated component, but this is considered acceptable error.

void DecomposeSeparableSunAndSkyIrradiance(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture2D irradianceLut, SamplerState lutSampler,
	float3 position, float3 sunDirection, out float3 separatedSunIrradiance, out float3 separatedSkyIrradiance)
{
	float radius = length(position);
	float muS = dot(position, sunDirection) / radius;
	
	// Separable forms of the equations found in GetSunAndSkyIrradiance
	separatedSunIrradiance = atmosphere.solarIrradiance * GetTransmittanceToSun(atmosphere, transmittanceLut, lutSampler, radius, muS);
	separatedSkyIrradiance = GetIrradiance(atmosphere, irradianceLut, lutSampler, radius, muS) * 0.5f;
}

void RecomposeSeparableSunAndSkyIrradiance(float3 position, float3 normal, float3 sunDirection, float3 separatedSunIrradiance, float3 separatedSkyIrradiance,
	out float3 sunIrradiance, out float3 skyIrradiance)
{
	float radius = length(position);
	
	sunIrradiance = separatedSunIrradiance * max(dot(normal, sunDirection), 0.f);
	skyIrradiance = separatedSkyIrradiance * (1.f + dot(normal, position) / radius);
}

float3 GetSolarRadiance(AtmosphereData atmosphere)
{
	return atmosphere.solarIrradiance / (pi * sunAngularRadius * sunAngularRadius);
}

float4 GetPlanetSurfaceRadiance(AtmosphereData atmosphere, float3 planetCenter, float3 cameraPosition, float3 rayDirection,
	float shadowStart, float shadowLength,
	float3 sunDirection, Texture2D transmittanceLut, Texture3D scatteringLut, Texture2D irradianceLut, SamplerState lutSampler)
{
	float3 p = cameraPosition - planetCenter;
	float pDotRay = dot(p, rayDirection);
	float intersectionDistance = -pDotRay - sqrt(planetCenter.z * planetCenter.z - (dot(p, p) - (pDotRay * pDotRay)));

	if (intersectionDistance > 0.f)
	{
		float3 surfacePoint = cameraPosition + rayDirection * intersectionDistance;
		float3 surfaceNormal = normalize(surfacePoint - planetCenter);

		float3 sunIrradiance;
		float3 skyIrradiance;
		GetSunAndSkyIrradiance(atmosphere, transmittanceLut, irradianceLut, lutSampler, surfacePoint - planetCenter, surfaceNormal, sunDirection, sunIrradiance, skyIrradiance);

		// #TODO: Compute approximate visibilities.
		// Sun visibility greatly reduces the brightness of the planet surface, sky visibility has little impact.
		float sunVisibility = 1.f;
		float skyVisibility = 1.f;

		float3 radiance = atmosphere.surfaceColor * (1.f / pi) * ((sunIrradiance * sunVisibility) + (skyIrradiance * skyVisibility));

		float3 transmittance;
		// The endpoint here is the actual planet surface, so endpointIsGround = true.
		float3 scattering = GetSkyRadianceToPoint(atmosphere, transmittanceLut, scatteringLut, lutSampler, cameraPosition - planetCenter, surfacePoint - planetCenter, true, shadowStart, shadowLength, sunDirection, transmittance);

		return float4(radiance * transmittance + scattering, 1.f);
	}

	return 0.f;
}

float3 ComputeAtmosphereCameraPosition(Camera camera)
{
	return camera.position.xyz / 1000.f;  // Atmosphere distances work in terms of kilometers due to floating point precision, so convert.
}

float3 ComputeAtmospherePlanetCenter(AtmosphereData atmosphere)
{
	// The planet center was pulled out from relying on the atmosphere model, as it's used in a variety of places.
	return planetCenter;
	
	//return float3(0.f, 0.f, -atmosphere.radiusBottom);  // World origin is planet surface.
}

float3 SampleAtmosphere(AtmosphereData atmosphere, Camera camera, float3 direction, float3 sunDirection, bool directSolarRadiance, Texture2D transmittanceLut, Texture3D scatteringLut, Texture2D irradianceLut, SamplerState lutSampler)
{
	float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	float3 planetCenter = ComputeAtmospherePlanetCenter(atmosphere);

	// IBL doesn't support light shafts, likely unnecessary.
	const float shadowStart  = 0.f;
	const float shadowLength = 0.f;

	float3 transmittance;
	float3 radiance = GetSkyRadiance(atmosphere, transmittanceLut, scatteringLut, lutSampler, cameraPosition - planetCenter, direction, shadowStart, shadowLength, sunDirection, transmittance);

	// If the ray intersects the sun, add solar radiance.
	// We don't want this for specular IBL, since the sun has immense radiant energy that will not
	// be represented properly in the prefilter map. Instead, the specular highlight of the sun is
	// contributed by a directional light with matching radiance.
	if (directSolarRadiance && dot(direction, sunDirection) > cos(sunAngularRadius))
	{
		radiance += transmittance * GetSolarRadiance(atmosphere);
	}

	float4 planetRadiance = GetPlanetSurfaceRadiance(atmosphere, planetCenter, cameraPosition, direction, shadowStart, shadowLength, sunDirection, transmittanceLut, scatteringLut, irradianceLut, lutSampler);
	radiance = lerp(radiance, planetRadiance.xyz, planetRadiance.w);

	// Native units, apply exposure in post.
	return radiance;
}

#endif  // __ATMOSPHERE_HLSLI__