// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Camera.hlsli"

struct BindData
{
    AtmosphereData atmosphere;
	// Boundary
    uint transmissionTexture;
    uint scatteringTexture;
    uint irradianceTexture;
    float solarZenithAngle;
    // Boundary
    uint sunTransmittanceBuffer;
    uint cameraBuffer;
    uint cameraIndex;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(1, 1, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

    float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	float3 planetCenter = ComputeAtmospherePlanetCenter(bindData.atmosphere);
	float3 direction = sunDirection;
	
    Texture2D<float4> transmittanceLut = ResourceDescriptorHeap[bindData.transmissionTexture];
    Texture3D<float4> scatteringLut = ResourceDescriptorHeap[bindData.scatteringTexture];
	Texture2D<float4> irradianceLut = ResourceDescriptorHeap[bindData.irradianceTexture];
	
	// Calculate the radiance and transmittance from the sky down to the planet surface. This is an approximation,
	// and will not be accurate for geometry being lit by the sun off of the surface.
	float3 transmittance;
	float3 skyRadiance = GetSkyRadiance(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearClamp, cameraPosition - planetCenter, direction, 0.f, sunDirection, transmittance);
    float3 solarRadiance = GetSolarRadiance(bindData.atmosphere);
	
	// #TEMP: This is an awful hack, not sure where the problem in my math is but there is way too much energy without this.
	// Revisit ASAP.
    const float radianceMultiplier = 0.001;
	
	RWStructuredBuffer<float3> sunTransmittance = ResourceDescriptorHeap[bindData.sunTransmittanceBuffer];
	sunTransmittance[0] = transmittance;
    sunTransmittance[1] = skyRadiance * radianceMultiplier;
    sunTransmittance[2] = solarRadiance * radianceMultiplier;
}