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
	
	float3 transmittance;
	float3 radiance = GetSkyRadiance(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearClamp, cameraPosition - planetCenter, direction, 0.f, sunDirection, transmittance);
	
	RWStructuredBuffer<float3> sunTransmittance = ResourceDescriptorHeap[bindData.sunTransmittanceBuffer];
	sunTransmittance[0] = transmittance;
	sunTransmittance[1] = radiance;
}