// Copyright (c) 2019-2021 Andrew Depke

#include "Atmosphere_RS.hlsli"
#include "Base.hlsli"
#include "Atmosphere.hlsli"
#include "Camera.hlsli"

ConstantBuffer<Camera> camera : register(b0);

SamplerState lutSampler : register(s0);

struct AtmosphereBindData
{
    AtmosphereData atmosphere;
    // Boundary
    uint transmissionTexture;
    uint scatteringTexture;
    uint irradianceTexture;
    float solarZenithAngle;
};

ConstantBuffer<AtmosphereBindData> bindData : register(b1);

struct Input
{
    float4 positionCS : SV_POSITION;
    float2 uv : UV;
};

struct Output
{
    float4 color : SV_Target;
};

float4 GetPlanetSurfaceRadiance(float3 planetCenter, float3 cameraPosition, float3 rayDirection, float3 sunDirection, Texture2D transmittanceLut, Texture3D scatteringLut, Texture2D irradianceLut)
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
        GetSunAndSkyIrradiance(bindData.atmosphere, transmittanceLut, irradianceLut, lutSampler, surfacePoint - planetCenter, surfaceNormal, sunDirection, sunIrradiance, skyIrradiance);
        
        float sunVisibility = 1.f;  // Light shafts are not yet supported.
        float skyVisibility = 1.f;  // Light shafts are not yet supported.
        float3 radiance = bindData.atmosphere.surfaceColor * (1.f / pi) * (sunIrradiance * sunVisibility) + (skyIrradiance * skyVisibility);
        
        float shadowLength = 0.f;  // Light shafts are not yet supported.
        float3 transmittance;
        float3 scattering = GetSkyRadianceToPoint(bindData.atmosphere, transmittanceLut, scatteringLut, lutSampler, cameraPosition - planetCenter, surfacePoint - planetCenter, shadowLength, sunDirection, transmittance);
        
        return float4(radiance * transmittance + scattering, 1.f);
    }
    
    return 0.f;
}

[RootSignature(RS)]
Output main(Input input)
{   
    float3 cameraPosition = camera.position.xyz / 1000.f;  // Atmosphere distances work in terms of kilometers due to floating point precision, so convert.
    float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
    float3 planetCenter = float3(0.f, 0.f, -bindData.atmosphere.radiusBottom);  // World origin is planet surface.
    
    float3 rayDirection = ComputeRayDirection(camera, input.uv);
    
    Texture2D transmittanceLut = textures[bindData.transmissionTexture];
    Texture3D scatteringLut = textures3D[bindData.scatteringTexture];
    Texture2D irradianceLut = textures[bindData.irradianceTexture];
    
    float transmittance;
    float3 radiance = GetSkyRadiance(bindData.atmosphere, transmittanceLut, scatteringLut, lutSampler, cameraPosition - planetCenter, rayDirection, 0.f, sunDirection, transmittance);
    
    // If the ray intersects the sun, add solar radiance.
    if (dot(rayDirection, sunDirection) > cos(sunAngularRadius))
    {
        radiance += transmittance * GetSolarRadiance(bindData.atmosphere);
    }
    
    float4 planetRadiance = GetPlanetSurfaceRadiance(planetCenter, cameraPosition, rayDirection, sunDirection, transmittanceLut, scatteringLut, irradianceLut);
    radiance = lerp(radiance, planetRadiance.xyz, planetRadiance.w);
    
    Output output;
    output.color.rgb = 1.f - exp(-radiance * 10.f);
    output.color.a = 1.f;
    
    return output;
}