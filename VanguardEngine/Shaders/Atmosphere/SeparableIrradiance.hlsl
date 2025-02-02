// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Camera.hlsli"
#include "Constants.hlsli"

struct BindData
{
    AtmosphereData atmosphere;
	// Boundary
    uint transmissionTexture;
    uint scatteringTexture;
    uint irradianceTexture;
    float solarZenithAngle;
    // Boundary
    uint atmosphereIrradianceBuffer;
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
	Texture2D<float4> irradianceLut = ResourceDescriptorHeap[bindData.irradianceTexture];
	
	// Calculate the atmosphere irradiance from the sky down to the planet surface. This is an approximation,
	// and will not be completely accurate for geometry being lit by the sun off of the surface.
    float3 separatedSunIrradianceNearCamera;
    float3 separatedSkyIrradianceNearCamera;
    DecomposeSeparableSunAndSkyIrradiance(bindData.atmosphere, transmittanceLut, irradianceLut, bilinearClamp, cameraPosition - planetCenter, sunDirection, separatedSunIrradianceNearCamera, separatedSkyIrradianceNearCamera);
	
    // Halfway through the cloud layer. Centering X/Y coordinates on the spectator camera should not have an affect, as I believe
    // only the height off the surface would impact the irradiance.
    float3 positionCloudLayer = float3(0.f, 0.f, cloudLayerBottom + 0.5f * (cloudLayerTop - cloudLayerBottom));
    
	// Calculate the atmosphere irradiance from the sky down to the middle of the cloud layer, for an improved approximation of light received.
    float3 separatedSunIrradianceClouds;
    float3 separatedSkyIrradianceClouds;
    DecomposeSeparableSunAndSkyIrradiance(bindData.atmosphere, transmittanceLut, irradianceLut, bilinearClamp, positionCloudLayer - planetCenter, sunDirection, separatedSunIrradianceClouds, separatedSkyIrradianceClouds);
	
    // Multiply the exposure in here since the separated irradiance is only ever used outside of the main atmosphere compose.
    RWStructuredBuffer<float3> atmosphereIrradiance = ResourceDescriptorHeap[bindData.atmosphereIrradianceBuffer];
    atmosphereIrradiance[0] = separatedSunIrradianceNearCamera * atmosphereRadianceExposure;
    atmosphereIrradiance[1] = separatedSkyIrradianceNearCamera * atmosphereRadianceExposure;
    atmosphereIrradiance[2] = separatedSunIrradianceClouds * atmosphereRadianceExposure;
    atmosphereIrradiance[3] = separatedSkyIrradianceClouds * atmosphereRadianceExposure;
}