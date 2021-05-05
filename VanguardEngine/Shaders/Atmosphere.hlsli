// Copyright (c) 2019-2021 Andrew Depke

#ifndef __ATMOSPHERE_HLSLI__
#define __ATMOSPHERE_HLSLI__

#include "Constants.hlsli"

// Atmospheric scattering, inspired by "Precomputed Atmospheric Rendering" [https://hal.inria.fr/inria-00288758/en]
// and with additions from Frostbite [https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf]

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
    float radius;  // Planet center to the start of the atmosphere.
    float3 padding0;
    
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
static const float minMuS = -0.2f;

float GetAtmosphereOuterRadius(AtmosphereData atmosphere)
{
    return atmosphere.radius + atmosphere.rayleighDensity.width;  // Air molecules have the least mass, so they extend much futher out into space than aerosols.
}

float DistanceToAtmosphereTop(AtmosphereData atmosphere, float radius, float mu)
{
    float top = GetAtmosphereOuterRadius(atmosphere);
    float discriminant = radius * radius * (mu * mu - 1.f) + top * top;
    return max(-radius * mu + sqrt(max(discriminant, 0.f)), 0.f);
}

float DistanceToAtmosphereBottom(AtmosphereData atmosphere, float radius, float mu)
{
    float bottom = atmosphere.radius;
    float discriminant = radius * radius + (mu * mu - 1.f) + bottom * bottom;
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
    return mu < 0.f && radius * radius * (mu * mu - 1.f) + atmosphere.radius * atmosphere.radius >= 0.f;
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
        float y_i = GetAtmosphereLayerDensity(layer, r_i - atmosphere.radius);
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
    float atmosphereOuterRadius = GetAtmosphereOuterRadius(atmosphere);
    float atmosphereOuterRadiusSquared = atmosphereOuterRadius * atmosphereOuterRadius;
    
    float h = sqrt(atmosphereOuterRadiusSquared - atmosphere.radius * atmosphere.radius);
    float rho = sqrt(max(radius * radius - atmosphereOuterRadiusSquared, 0.f));
    float d = DistanceToAtmosphereTop(atmosphere, radius, mu);
    float dMin = atmosphereOuterRadius - radius;
    float dMax = h + rho;
    float xMu = (d - dMin) / (dMax - dMin);
    float xRadius = rho / h;
    
    return float2(UnitRangeToTextureCoord(xMu, textureSize.x), UnitRangeToTextureCoord(xRadius, textureSize.y));
}

float2 GetTransmittanceLutData(AtmosphereData atmosphere, float2 uv, float2 textureSize)
{
    float xMu = TextureCoordToUnitRange(uv.x, textureSize.x);
    float xRadius = TextureCoordToUnitRange(uv.y, textureSize.y);
    
    float atmosphereOuterRadius = GetAtmosphereOuterRadius(atmosphere);
    float atmosphereOuterRadiusSquared = atmosphereOuterRadius * atmosphereOuterRadius;
    float atmosphereRadiusSquared = atmosphere.radius * atmosphere.radius;
    
    float h = sqrt(atmosphereOuterRadiusSquared - atmosphereRadiusSquared);
    float rho = h * xRadius;
    float radius = sqrt(rho * rho + atmosphereRadiusSquared);
    float dMin = atmosphereOuterRadius - radius;
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
    float radius_d = clamp(sqrt(d * d + 2.f * radius * mu * d + radius * radius), atmosphere.radius, GetAtmosphereOuterRadius(atmosphere));
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

float3 GetTransmittanceToSun(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float radius, float mu_s)
{
    float sinThetaH = atmosphere.radius / radius;
    float cosThetaH = -sqrt(max(1.f - sinThetaH * sinThetaH, 0.f));
    
    return GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius, mu_s) * smoothstep(-sinThetaH * sunAngularRadius, sinThetaH * sunAngularRadius, mu_s - cosThetaH);
}

void ComputeSingleScatteringIntegrand(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float radius, float mu, float mu_s, float nu, float d, bool rayIntersectsGround, out float3 rayleigh, out float3 mie)
{
    float radius_d = clamp(sqrt(d * d + 2.f * radius * mu * d + radius * radius), atmosphere.radius, GetAtmosphereOuterRadius(atmosphere));
    float mu_s_d = clamp((radius * mu_s + d * nu) / radius_d, -1.f, 1.f);
    float3 transmittance = GetTransmittance(atmosphere, transmittanceLut, lutSampler, radius, mu, d, rayIntersectsGround) * GetTransmittanceToSun(atmosphere, transmittanceLut, lutSampler, radius_d, mu_s_d);
    
    rayleigh = transmittance * GetAtmosphereLayerDensity(atmosphere.rayleighDensity, radius_d - atmosphere.radius);
    mie = transmittance * GetAtmosphereLayerDensity(atmosphere.mieDensity, radius_d - atmosphere.radius);
}

void ComputeSingleScattering(AtmosphereData atmosphere, Texture2D transmittanceLut, SamplerState lutSampler, float radius, float mu, float mu_s, float nu, bool rayIntersectsGround, out float3 rayleigh, out float3 mie)
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
        ComputeSingleScatteringIntegrand(atmosphere, transmittanceLut, lutSampler, radius, mu, mu_s, nu, d_i, rayIntersectsGround, rayleigh_i, mie_i);
        float weight_i = (i == 0 || i == steps) ? 0.5f : 1.f;
        
        rayleighSum += rayleigh_i * weight_i;
        mieSum += mie_i * weight_i;
    }
    
    rayleigh = rayleighSum * dx * atmosphere.solarIrradiance * atmosphere.rayleighScattering;
    mie = mieSum * dx * atmosphere.solarIrradiance * atmosphere.mieScattering;
}

float RayleighPhase(float nu)
{
    static const float k = 3.f / (16.f * pi);
    return k * (1.f + nu * nu);
}

float MiePhase(float nu)
{
    // Mean cosine, determines the medium anisotropy. 0 = isotropic.
    static const float g = 0.76f;
    
    float gSquared = g * g;
    float k = 3.f / (8.f * pi) * (1.f - gSquared) / (2.f + gSquared);
    return k * (1.f + nu * nu) / pow(1.f + gSquared - 2.f * g * nu, 1.5f);
}

float4 GetScatteringLutDimensions4D(float3 textureSize)
{
    static const float nuSize = 8.f;
    
    // Radius, mu, muS, nu.
    return float4(textureSize.z, textureSize.y, textureSize.x / nuSize, nuSize);
}

float4 GetScatteringLutCoord(AtmosphereData atmosphere, float radius, float mu, float muS, float nu, bool rayIntersectsGround, float4 textureSize)
{
    float atmosphereOuterRadius = GetAtmosphereOuterRadius(atmosphere);
    float atmosphereOuterRadiusSquared = atmosphereOuterRadius * atmosphereOuterRadius;
    float atmosphereRadiusSquared = atmosphere.radius * atmosphere.radius;
    float radiusSquared = radius * radius;
    
    float h = sqrt(atmosphereOuterRadiusSquared - atmosphereRadiusSquared);
    float rho = sqrt(max(radiusSquared - atmosphereRadiusSquared, 0.f));
    float uRadius = UnitRangeToTextureCoord(rho / h, textureSize.x);
    float radiusMu = radius * mu;
    float discriminant = radiusMu * radiusMu - radiusSquared + atmosphereRadiusSquared;
    
    float uMu;
    if (rayIntersectsGround)
    {
        float d = -radiusMu - sqrt(max(discriminant, 0.f));
        float dMin = radius - atmosphere.radius;
        float dMax = rho;
        uMu = 0.5f - 0.5f * UnitRangeToTextureCoord(dMax == dMin ? 0.f : (d - dMin) / (dMax - dMin), textureSize.y / 2.f);
    }
    
    else
    {
        float d = -radiusMu + sqrt(max(discriminant + h * h, 0.f));
        float dMin = atmosphereOuterRadius - radius;
        float dMax = h + rho;
        uMu = 0.5f + 0.5f * UnitRangeToTextureCoord((d - dMin) / (dMax - dMin), textureSize.y / 2.f);
    }
    
    float d = DistanceToAtmosphereTop(atmosphere, atmosphere.radius, muS);
    float dMin = atmosphereOuterRadius - atmosphere.radius;
    float dMax = h;
    float a = (d - dMin) / (dMax - dMin);
    float D = DistanceToAtmosphereTop(atmosphere, atmosphere.radius, minMuS);
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
    float atmosphereOuterRadius = GetAtmosphereOuterRadius(atmosphere);
    float atmosphereOuterRadiusSquared = atmosphereOuterRadius * atmosphereOuterRadius;
    float atmosphereRadiusSquared = atmosphere.radius * atmosphere.radius;
    
    float h = sqrt(atmosphereOuterRadiusSquared - atmosphereRadiusSquared);
    float rho = h * TextureCoordToUnitRange(uvwz.w, textureSize.x);
    
    ScatteringLutData lutData;
    lutData.radius = sqrt(rho * rho + atmosphereRadiusSquared);
    
    if (uvwz.z < 0.5f)
    {
        float dMin = lutData.radius - atmosphere.radius;
        float dMax = rho;
        float d = dMin + (dMax - dMin) * TextureCoordToUnitRange(1.f - 2.f * uvwz.z, textureSize.y / 2.f);
        lutData.mu = d == 0.f ? -1.f : clamp(-(rho * rho + d * d) / (2.f * lutData.radius * d), -1.f, 1.f);
        lutData.rayIntersectsGround = true;
    }
    
    else
    {
        float dMin = atmosphereOuterRadius - lutData.radius;
        float dMax = h + rho;
        float d = dMin + (dMax - dMin) * TextureCoordToUnitRange(2.f * uvwz.z - 1.f, textureSize.y / 2.f);
        lutData.mu = d == 0.f ? 1.f : clamp((h * h - rho * rho - d * d) / (2.f * lutData.radius * d), -1.f, 1.f);
        lutData.rayIntersectsGround = false;
    }
    
    float xMuS = TextureCoordToUnitRange(uvwz.y, textureSize.z);
    float dMin = atmosphereOuterRadius - atmosphere.radius;
    float dMax = h;
    float D = DistanceToAtmosphereTop(atmosphere, atmosphere.radius, minMuS);
    float A = (D - dMin) / (dMax - dMin);
    float a = (A - xMuS * A) / (1.f + xMuS * A);
    float d = dMin + min(a, A) * (dMax - dMin);
    lutData.muS = d == 0.f ? 1.f : clamp((h * h - d * d) / (2.f * atmosphere.radius * d), -1.f, 1.f);
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
    float lutCoordX = uvwz.x * textureSize.w - 1.f;
    float lutX = floor(lutCoordX);
    float lerp = lutCoordX - lutX;
    float3 uvw0 = float3((lutX + uvwz.y) / textureSize.w, uvwz.z, uvwz.w);
    float3 uvw1 = float3((lutX + 1.f + uvwz.y) / textureSize.w, uvwz.z, uvwz.w);
    
    return scatteringLut.SampleLevel(lutSampler, uvw0, 0).xyz * (1.f - lerp) + scatteringLut.SampleLevel(lutSampler, uvw1, 0).xyz * lerp;
}

float3 GetScattering(AtmosphereData atmosphere, Texture3D singleRayleighScatteringLut, Texture3D singleMieScatteringLut, SamplerState lutSampler, float radius, float mu, float muS, float nu, bool rayIntersectsGround, int scatteringOrder)
{
    if (scatteringOrder == 1)
    {
        float3 rayleigh = GetScattering(atmosphere, singleRayleighScatteringLut, lutSampler, radius, mu, muS, nu, rayIntersectsGround);
        float3 mie = GetScattering(atmosphere, singleMieScatteringLut, lutSampler, radius, mu, muS, nu, rayIntersectsGround);
        
        return rayleigh * RayleighPhase(nu) + mie * MiePhase(nu);
    }
    
    else
    {
        return 0.f;  // #TODO: Multiple scattering is not supported.
    }
}
/*
float3 ComputeDirectIrradiance(AtmosphereData atmosphere, float radius, float muS)
{
    float alphaS = sunAngularRadius;
    float avgCosFactor = muS < -alphaS ? 0.f : (muS > alphaS ? muS : (muS + alphaS) * (muS + alphaS) / (4.f * alphaS));

    return atmosphere.solarIrradiance * ComputeTransmittanceToAtmosphereTop(atmosphere, radius, muS) * avgCosFactor;
}

float3 ComputeIndirectIrradiance(AtmosphereData atmosphere, float radius, float muS, int scatteringOrder)
{
    //static const int steps = 32;
    static const int steps = 4;  // #TEMP: Indirect irradiance is extremely expensive.
    float dx = pi / steps;
    
    float3 result = 0.f;
    float3 omegaS = float3(sqrt(1.f - muS * muS), 0.f, muS);
    
    for (int i = 0; i < steps / 2; ++i)
    {
        float theta = (i + 0.5f) * dx;

        for (int j = 0; j < steps * 2; ++j)
        {
            float phi = (j + 0.5f) * dx;
            float3 omega = float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
            float dOmega = dx * dx * sin(theta);
            float nu = dot(omega, omegaS);
            
            result += GetScattering(atmosphere, radius, omega.z, muS, nu, false, scatteringOrder);
        }
    }
    
    return result;
}
*/
float3 GetSkyRadiance(AtmosphereData atmosphere, Texture2D transmittanceLut, Texture3D singleRayleighScatteringLut, Texture3D singleMieScatteringLut, SamplerState lutSampler, float3 cameraPosition, float3 viewDirection, float shadowLength, float3 sunDirection, out float3 transmittance)
{
    float radius = length(cameraPosition);
    float rMu = dot(cameraPosition, viewDirection);
    float distanceToAtmosphereTop = -rMu - sqrt(rMu * rMu - radius * radius + GetAtmosphereOuterRadius(atmosphere) * GetAtmosphereOuterRadius(atmosphere));
    
    // If the camera is in space, move the position to be at the atmosphere top.
    if (distanceToAtmosphereTop > 0.f)
    {
        cameraPosition = cameraPosition + viewDirection * distanceToAtmosphereTop;
        radius = GetAtmosphereOuterRadius(atmosphere);
        rMu += distanceToAtmosphereTop;
    }
    
    // View ray doesn't intersect the atmosphere.
    else if (radius > GetAtmosphereOuterRadius(atmosphere))
    {        
        transmittance = 1.f;
        return 0.f;
    }
    
    float mu = rMu / radius;
    float muS = dot(cameraPosition, sunDirection) / radius;
    float nu = dot(viewDirection, sunDirection);
    bool rayIntersectsGround = RayIntersectsGround(atmosphere, radius, mu);
    
    transmittance = rayIntersectsGround ? 0.f : GetTransmittanceToAtmosphereTop(atmosphere, transmittanceLut, lutSampler, radius, mu);
    
    // ...
    
    return GetScattering(atmosphere, singleRayleighScatteringLut, singleMieScatteringLut, lutSampler, radius, mu, muS, nu, rayIntersectsGround, 1);
}
/*
void GetSunAndSkyIrradiance(AtmosphereData atmosphere, float3 position, float3 normal, float3 sunDirection, out float3 sunIrradiance, out float3 skyIrradiance)
{
    float radius = length(position);
    float muS = dot(position, sunDirection) / radius;
    
    // Direct irradiance.
    sunIrradiance = atmosphere.solarIrradiance * GetTransmittanceToSun(atmosphere, radius, muS) * max(dot(normal, sunDirection), 0.f);
    // Indirect irradiance.
    skyIrradiance = ComputeIndirectIrradiance(atmosphere, radius, muS, 1) * (1.f + dot(normal, position) / radius) * 0.5f;
}
*/
float3 GetSolarRadiance(AtmosphereData atmosphere)
{
    return atmosphere.solarIrradiance / (pi * sunAngularRadius * sunAngularRadius);
}

#endif  // __ATMOSPHERE_HLSLI__