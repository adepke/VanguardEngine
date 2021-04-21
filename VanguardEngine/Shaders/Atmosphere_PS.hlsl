// Copyright (c) 2019-2021 Andrew Depke

#include "Atmosphere_RS.hlsli"
#include "Atmosphere.hlsli"
#include "Camera.hlsli"

ConstantBuffer<Camera> camera : register(b0);

struct Input
{
    float4 positionCS : SV_POSITION;
    float2 uv : UV;
};

struct Output
{
    float4 color : SV_Target;
};

[RootSignature(RS)]
Output main(Input input)
{
    // Meters.
    float3 rayleighScattering = float3(0.0000058f, 0.0000135f, 0.0000331f);  // Frostbite's.
    float mieScattering = 0.000004f;
    float mieExtinction = 1.11f * mieScattering;  // Frostbite's.
    float ozoneAbsorption = float3(0.0000020556f, 0.0000049788f, 0.0000002136f);  // Frostbite's.
    
    // Atmosphere contribution.
    AtmosphereData atmosphere;
    atmosphere.radius = 6360000.f;
     atmosphere.rayleighDensity.width = 60000.f;
    atmosphere.rayleighDensity.exponentialCoefficient = 1.f;
    atmosphere.rayleighDensity.exponentialScale = -1.f / 8000.f;
    atmosphere.rayleighDensity.heightScale = 0.f;
    atmosphere.rayleighDensity.offset = 0.f;
    atmosphere.rayleighScattering = rayleighScattering;
     atmosphere.mieDensity.width = 20000.f;
    atmosphere.mieDensity.exponentialCoefficient = 1.f;
    atmosphere.mieDensity.exponentialScale = -1.f / 1200.f;
    atmosphere.mieDensity.heightScale = 0.f;
    atmosphere.mieDensity.offset = 0.f;
    atmosphere.mieScattering = mieScattering.xxx;
    atmosphere.mieExtinction = mieExtinction.xxx;
    atmosphere.absorptionDensity.width = 25000.f;
    atmosphere.absorptionDensity.exponentialCoefficient = 0.f;
    atmosphere.absorptionDensity.exponentialScale = 0.f;
    atmosphere.absorptionDensity.heightScale = 1.f / 15000.f;
    atmosphere.absorptionDensity.offset = -2.f / 3.f;
    atmosphere.absorptionExtinction = ozoneAbsorption;
    atmosphere.surfaceColor = float3(0.1f, 0.1f, 0.1f);
    atmosphere.solarIrradiance = float3(1.474f, 1.8504f, 1.91198f);
    
    float3 sunDirection = normalize(float3(1.f, 0.f, 0.6f));
    float3 planetCenter = float3(0.f, 0.f, -atmosphere.radius);  // World origin is planet surface.
    
    float3 rayDirection = ComputeRayDirection(camera, input.uv);
    
    float transmittance;
    float3 radiance = GetSkyRadiance(atmosphere, camera.position.xyz - planetCenter, rayDirection, 0.f, sunDirection, transmittance);
    
    // If the ray intersects the sun, add solar radiance.
    if (dot(rayDirection, sunDirection) > cos(sunAngularRadius))
    {
        radiance += transmittance * GetSolarRadiance(atmosphere);
    }
    
    Output output;
    output.color.rgb = 1.f - exp(-radiance * 1.85f);
    output.color.a = 1.f;
    
    return output;
}