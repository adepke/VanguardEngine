// Copyright (c) 2019-2021 Andrew Depke

#include "AtmospherePrecompute_RS.hlsli"
#include "Base.hlsli"
#include "Atmosphere.hlsli"

struct PrecomputeData
{
    AtmosphereData atmosphere;
    uint transmissionTexture;
    uint scatteringTexture;
    uint irradianceTexture;
    float padding;
};

ConstantBuffer<PrecomputeData> precomputeData : register(b0);

SamplerState lutSampler : register(s0);

struct Input
{
    uint groupIndex : SV_GroupIndex;
    uint3 dispatchId : SV_DispatchThreadID;
};

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void TransmittanceLutMain(Input input)
{
    RWTexture2D<float4> transmittance = texturesRW[precomputeData.transmissionTexture];
    
    float2 dimensions;
    transmittance.GetDimensions(dimensions.x, dimensions.y);
    float2 uv = float2(input.dispatchId.x / dimensions.x, input.dispatchId.y / dimensions.y);
    
    float3 transmittanceData = ComputeTransmittanceToAtmosphereTopLut(precomputeData.atmosphere, uv, dimensions);
    transmittance[input.dispatchId.xy] = float4(transmittanceData, 1.f);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void ScatteringLutMain(Input input)
{
    Texture2D<float4> transmittance = textures[precomputeData.transmissionTexture];
    RWTexture3D<float4> scattering = textures3DRW[precomputeData.scatteringTexture];
    
    float3 dimensions;
    scattering.GetDimensions(dimensions.x, dimensions.y, dimensions.z);
    float3 uvw = float3(input.dispatchId.x / dimensions.x, input.dispatchId.y / dimensions.y, input.dispatchId.z / dimensions.z);
    
    float3 rayleigh;
    float3 mie;
    ComputeSingleScatteringLut(precomputeData.atmosphere, transmittance, lutSampler, uvw, dimensions, rayleigh, mie);
    scattering[input.dispatchId.xyz] = float4(rayleigh, mie.r);
}