// Copyright (c) 2019-2021 Andrew Depke

#ifndef __ATMOSPHERE_HLSLI__
#define __ATMOSPHERE_HLSLI__

#include "Constants.hlsli"

// Atmospheric scattering, inspired by "Precomputed Atmospheric Rendering" [https://hal.inria.fr/inria-00288758/en]
// and with additions by Frostbite [https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf]

// Density = exponentialCoefficient * exp(exponentialScale * height) + heightScale * height + offset
struct DensityLayer
{
    float width;
    float exponentialCoefficient;
    float exponentialScale;
    float heightScale;
    float offset;
};

struct AtmosphereData
{
    float radius;  // Planet center to the start of the atmosphere.
    
    DensityLayer rayleighDensity;
    float3 rayleighScattering;  // Air molecule scattering, absorption is considered negligible.
    
    DensityLayer mieDensity;
    float3 mieScattering;
    float3 mieExtinction;
    
    DensityLayer absorptionDensity;
    float3 absorptionExtinction;
    
    float3 surfaceColor;  // Average albedo of the planet surface.
    
    float3 solarIrradiance;  // #TODO: Separate sun data out of the atmosphere.
};

static const float sunAngularRadius = 0.004675f;

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
    //static const int steps = 500;
    static const int steps = 20;  // #TEMP
    
    float dx = DistanceToAtmosphereTop(atmosphere, radius, mu) / steps;
    float result = 0.f;
    
    [unroll]
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

float3 GetTransmittance(AtmosphereData atmosphere, float radius, float mu, float d, bool rayIntersectsGround)
{
    float minTransmittance = 1.f;
    float radius_d = clamp(sqrt(d * d + 2.f * radius * mu * d + radius * radius), atmosphere.radius, GetAtmosphereOuterRadius(atmosphere));
    float mu_d = clamp((radius * mu + d) / radius_d, -1.f, 1.f);
    
    if (rayIntersectsGround)
    {
        return min(ComputeTransmittanceToAtmosphereTop(atmosphere, radius_d, -mu_d) / ComputeTransmittanceToAtmosphereTop(atmosphere, radius, -mu), minTransmittance.xxx);
    }
    
    else
    {
        return min(ComputeTransmittanceToAtmosphereTop(atmosphere, radius, mu) / ComputeTransmittanceToAtmosphereTop(atmosphere, radius_d, mu_d), minTransmittance.xxx);
    }
}

float3 GetTransmittanceToSun(AtmosphereData atmosphere, float radius, float mu_s)
{
    float sinThetaH = atmosphere.radius / radius;
    float cosThetaH = -sqrt(max(1.f - sinThetaH * sinThetaH, 0.f));
    
    return ComputeTransmittanceToAtmosphereTop(atmosphere, radius, mu_s) * smoothstep(-sinThetaH * sunAngularRadius, sinThetaH * sunAngularRadius, mu_s - cosThetaH);
}

void ComputeSingleScatteringIntegrand(AtmosphereData atmosphere, float radius, float mu, float mu_s, float nu, float d, bool rayIntersectsGround, out float3 rayleigh, out float3 mie)
{
    float radius_d = clamp(sqrt(d * d + 2.f * radius * mu * d + radius * radius), atmosphere.radius, GetAtmosphereOuterRadius(atmosphere));
    float mu_s_d = clamp((radius * mu_s + d * nu) / radius_d, -1.f, 1.f);
    float3 transmittance = GetTransmittance(atmosphere, radius, mu, d, rayIntersectsGround) * GetTransmittanceToSun(atmosphere, radius_d, mu_s_d);
    
    rayleigh = transmittance * GetAtmosphereLayerDensity(atmosphere.rayleighDensity, radius_d - atmosphere.radius);
    mie = transmittance * GetAtmosphereLayerDensity(atmosphere.mieDensity, radius_d - atmosphere.radius);
}

void ComputeSingleScattering(AtmosphereData atmosphere, float radius, float mu, float mu_s, float nu, bool rayIntersectsGround, out float3 rayleigh, out float3 mie)
{
    // Intervals for numerical integration.
    //static const int steps = 50;
    static const int steps = 10; // #TEMP
    
    float dx = DistanceToNearestAtmosphereEdge(atmosphere, radius, mu, rayIntersectsGround) / steps;
    float3 rayleighSum = 0.f;
    float3 mieSum = 0.f;
    
    [unroll]
    for (int i = 0; i <= steps; ++i)
    {
        float d_i = i * dx;
        float3 rayleigh_i;
        float3 mie_i;
        ComputeSingleScatteringIntegrand(atmosphere, radius, mu, mu_s, nu, d_i, rayIntersectsGround, rayleigh_i, mie_i);
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

float3 GetScattering(AtmosphereData atmosphere, float radius, float mu, float muS, float nu, bool rayIntersectsGround, int scatteringOrder)
{
    if (scatteringOrder == 1)
    {
        float3 rayleigh;
        float3 mie;
        ComputeSingleScattering(atmosphere, radius, mu, muS, nu, rayIntersectsGround, rayleigh, mie);
        
        return rayleigh * RayleighPhase(nu) + mie * MiePhase(nu);
    }
    
    else
    {
        return 0.f;  // #TODO: Multiple scattering is not supported.
    }
}

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
    
    [unroll]
    for (int i = 0; i < steps / 2; ++i)
    {
        float theta = (i + 0.5f) * dx;

        [unroll]
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

float3 GetSkyRadiance(AtmosphereData atmosphere, float3 cameraPosition, float3 viewDirection, float shadowLength, float3 sunDirection, out float3 transmittance)
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
    
    transmittance = rayIntersectsGround ? 0.f : ComputeTransmittanceToAtmosphereTop(atmosphere, radius, mu);
    
    // ...
    
    return GetScattering(atmosphere, radius, mu, muS, nu, rayIntersectsGround, 1);
}

void GetSunAndSkyIrradiance(AtmosphereData atmosphere, float3 position, float3 normal, float3 sunDirection, out float3 sunIrradiance, out float3 skyIrradiance)
{
    float radius = length(position);
    float muS = dot(position, sunDirection) / radius;
    
    // Direct irradiance.
    sunIrradiance = atmosphere.solarIrradiance * GetTransmittanceToSun(atmosphere, radius, muS) * max(dot(normal, sunDirection), 0.f);
    // Indirect irradiance.
    skyIrradiance = ComputeIndirectIrradiance(atmosphere, radius, muS, 1) * (1.f + dot(normal, position) / radius) * 0.5f;
}

float3 GetSolarRadiance(AtmosphereData atmosphere)
{
    return atmosphere.solarIrradiance / (pi * sunAngularRadius * sunAngularRadius);
}

/*
struct AtmosphereContext
{
    float radius;
    float planetRadius;
    float resolutionHeight;
    float resolutionView;
    float resolutionSun;
};

struct AtmosphereLutParameter
{
    float height;
    float cosViewAngle;
    float cosSunAngle;
};

struct AtmosphereLutCoord
{
    float2 transmittance;  // 2D transmittance.
    float3 scattering;  // 3D scattering.
};

AtmosphereLutParameter ComputeLutParameter(AtmosphereContext context, )
{
    
}

AtmosphereLutCoord ComputeLutCoord(AtmosphereContext context, AtmosphereLutParameter parameters)
{
    float radiusDifference = context.radius - context.planetRadius;
    float normalizedHeight = clamp(parameters.height, 0.f, radiusDifference);
    normalizedHeight = saturate(normalizedHeight / radiusDifference);
    normalizedHeight = pow(normalizedHeight, 0.5f);
    
    float viewZenithTransmission = 0.5f * (atan(max(parameters.cosViewAngle, -0.45f) * tan(1.26f * 0.75f)) / 0.75f + (1.f - 0.26f));
    
    float height = max(parameters.height, 0.f);
    float cosHorizon = -sqrt(height * (2.f * context.planetRadius + height)) / (context.planetRadius + height);
    float viewZenithScattering;
    
    if (parameters.cosViewAngle > cosHorizon)
    {
        float cosViewAngle = max(parameters.cosViewAngle, cosHorizon + 0.0001f);
        viewZenithScattering = saturate((cosViewAngle - cosHorizon) / (1.f - cosHorizon));
        viewZenithScattering = pow(viewZenithScattering, 0.2f);
        viewZenithScattering = 0.5f + 0.5f / context.resolutionView + viewZenithScattering * (context.resolutionView / 2.f - 1.f) / context.resolutionView;
    }
    
    else
    {
        float cosViewAngle = min(parameters.cosViewAngle, cosHorizon - 0.0001f);
        viewZenithScattering = saturate((cosHorizon - cosViewAngle) / (cosHorizon + 1.f));
        viewZenithScattering = pow(viewZenithScattering, 0.2f);
        viewZenithScattering = 0.5f / context.resolutionView + viewZenithScattering * (context.resolutionView / 2.f - 1.f) / context.resolutionView;
    }
    
    float sunZenith = 0.5f * (atan(max(parameters.cosSunAngle, -0.45f) * tan(1.26f * 0.75f)) / 0.75f + (1.f - 0.26f));
    
    AtmosphereLutCoord result;
    result.transmittance = float2(normalizedHeight, viewZenithTransmission);
    result.transmittance = ((result.transmittance * (float2(context.resolutionHeight, context.resolutionView) - 1.f) + 0.5f) / float2(context.resolutionHeight, context.resolutionView));
    result.scattering = float3(normalizedHeight, viewZenithScattering, sunZenith);
    result.scattering.xz = ((result.scattering * (float3(context.resolutionHeight, context.resolutionView, context.resolutionSun) - 1.f) + 0.5f) / float3(context.resolutionHeight, context.resolutionView, context.resolutionSun)).xz;
    
    return result;
}
*/

#endif  // __ATMOSPHERE_HLSLI__