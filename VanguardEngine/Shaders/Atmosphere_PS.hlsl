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
    uint transmissionTexture;
    uint scatteringTexture;
    float2 padding;
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

[RootSignature(RS)]
Output main(Input input)
{   
    float3 sunDirection = normalize(float3(1.f, 0.f, 0.4f));
    float3 planetCenter = float3(0.f, 0.f, -bindData.atmosphere.radius);  // World origin is planet surface.
    
    float3 rayDirection = ComputeRayDirection(camera, input.uv);
    
    Texture2D transmittanceLut = textures[bindData.transmissionTexture];
    Texture3D scatteringLut = textures3D[bindData.scatteringTexture];
    
    float transmittance;
    float3 radiance = GetSkyRadiance(bindData.atmosphere, transmittanceLut, scatteringLut, scatteringLut, lutSampler, camera.position.xyz - planetCenter, rayDirection, 0.f, sunDirection, transmittance);
    
    // If the ray intersects the sun, add solar radiance.
    if (dot(rayDirection, sunDirection) > cos(sunAngularRadius))
    {
        radiance += transmittance * GetSolarRadiance(bindData.atmosphere);
    }
    
    Output output;
    output.color.rgb = 1.f - exp(-radiance * 1.85f);
    output.color.a = 1.f;
    
    return output;
}