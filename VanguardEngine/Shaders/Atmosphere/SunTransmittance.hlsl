// Copyright (c) 2019-2022 Andrew Depke

#include "Base.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Camera.hlsli"

#define RS \
	"RootFlags(0)," \
	"RootConstants(b1, num32BitConstants = 56)," \
	"CBV(b0)," \
	"UAV(u0)," \
	"DescriptorTable(" \
		"SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"StaticSampler(" \
		"s0," \
		"filter = FILTER_MIN_MAG_MIP_LINEAR," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP)"

struct AtmosphereBindData
{
    AtmosphereData atmosphere;
	// Boundary
    uint transmissionTexture;
    uint scatteringTexture;
    uint irradianceTexture;
    float solarZenithAngle;
};

ConstantBuffer<Camera> camera : register(b0);
ConstantBuffer<AtmosphereBindData> bindData : register(b1);
RWStructuredBuffer<float3> sunTransmittance : register(u0);
SamplerState lutSampler : register(s0);

[RootSignature(RS)]
[numthreads(1, 1, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
    float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	float3 planetCenter = ComputeAtmospherePlanetCenter(bindData.atmosphere);
	float3 direction = sunDirection;
	
    Texture2D transmittanceLut = textures[bindData.transmissionTexture];
    Texture3D scatteringLut;  // Unused.
    Texture2D irradianceLut;  // Unused.
	
	float3 transmittance;
	float3 radiance = GetSkyRadiance(bindData.atmosphere, transmittanceLut, scatteringLut, lutSampler, cameraPosition - planetCenter, direction, 0.f, sunDirection, transmittance);
	
	sunTransmittance[0] = transmittance;
}