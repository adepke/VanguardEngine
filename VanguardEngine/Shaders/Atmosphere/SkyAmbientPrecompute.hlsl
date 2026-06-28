// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/SkyAmbient.hlsli"
#include "Camera.hlsli"
#include "Constants.hlsli"

struct BindData
{
	AtmosphereData atmosphere;
	// Boundary
	uint transmissionTexture;
	uint scatteringTexture;
	float solarZenithAngle;
	uint atmosphereIrradianceBuffer;
	// Boundary
	uint cameraBuffer;
	uint cameraIndex;
};

ConstantBuffer<BindData> bindData : register(b0);

// Produces a near-uniform distribution suitable for Monte-Carlo integration of the sphere.
// #TODO: refactor into common utils.
float3 FibonacciSphere(int i, int N)
{
	const float goldenRatio = (1.0f + sqrt(5.0f)) * 0.5f;
	const float theta = 2.0f * pi * (float)i / goldenRatio;
	const float cosPhi = 1.0f - 2.0f * ((float)i + 0.5f) / (float)N;
	const float sinPhi = sqrt(max(1.0f - cosPhi * cosPhi, 0.0f));
	return float3(cos(theta) * sinPhi, sin(theta) * sinPhi, cosPhi);
}

// Sample the sky in a uniform sphere and project into spherical-harmonics. Increasing the sample count
// costs more sky samples, but increases projection quality.
SH::L2_RGB ProjectSkyOntoL2(AtmosphereData atmosphere, Texture2D<float4> transmittanceLut, Texture3D<float4> scatteringLut,
	SamplerState lutSampler, float3 probePosition, float3 sunDirection, int sampleCount)
{
	SH::L2_RGB sh = SH::L2_RGB::Zero();
	int usedSamples = 0;
	for (int i = 0; i < sampleCount; ++i)
	{
		float3 dir = FibonacciSphere(i, sampleCount);

		// Reject samples that align with the sun, this would skew the projection, and high frequency
		// signal does not mix well with SH. Direct sun contribution comes from a separate source instead.
		if (dot(dir, sunDirection) > cos(sunAngularRadius))
			continue;
		usedSamples += 1;

		float3 transmittance;
		float3 skyRadiance = GetSkyRadiance(atmosphere, transmittanceLut, scatteringLut, lutSampler,
			probePosition, dir, 0.f, 0.f, sunDirection, transmittance);

		sh = sh + SH::ProjectOntoL2(dir, skyRadiance);
	}

	// Monte-Carlo normalization.
	if (usedSamples > 0)
	{
		const float scale = (4.0f * pi) / (float)usedSamples;
		sh = sh * float3(scale, scale, scale);
	}
	return sh;
}

[RootSignature(RS)]
[numthreads(1, 1, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 cameraPosition = ComputeAtmosphereCameraPosition(camera);
	float3 planetCenter = ComputeAtmospherePlanetCenter(bindData.atmosphere);

	Texture2D<float4> transmittanceLut = ResourceDescriptorHeap[bindData.transmissionTexture];
	Texture3D<float4> scatteringLut = ResourceDescriptorHeap[bindData.scatteringTexture];

	// Capture direct sun radiance at the camera and in the clouds, which is not encoded in the SH probes.
	float3 separatedSunIrradianceNearCamera = GetSeparableSunIrradiance(bindData.atmosphere, transmittanceLut, bilinearClamp,
		cameraPosition - planetCenter, sunDirection);
	
	// Place the cloud probe halfway through the cloud layer. X/Y doesn't matter here.
	float3 positionCloudLayer = float3(0.f, 0.f, cloudLayerBottom + 0.5f * (cloudLayerTop - cloudLayerBottom));

	float3 separatedSunIrradianceClouds = GetSeparableSunIrradiance(bindData.atmosphere, transmittanceLut, bilinearClamp,
		positionCloudLayer - planetCenter, sunDirection);

	const int sphereSamples = 64;
	SH::L2_RGB skyCameraSH = ProjectSkyOntoL2(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearClamp,
		cameraPosition - planetCenter, sunDirection, sphereSamples);
	SH::L2_RGB skyCloudsSH = ProjectSkyOntoL2(bindData.atmosphere, transmittanceLut, scatteringLut, bilinearClamp,
		positionCloudLayer - planetCenter, sunDirection, sphereSamples);

	RWStructuredBuffer<float3> atmosphereIrradiance = ResourceDescriptorHeap[bindData.atmosphereIrradianceBuffer];
	atmosphereIrradiance[kAtmosphereIrradianceSunCameraIndex] = separatedSunIrradianceNearCamera;
	atmosphereIrradiance[kAtmosphereIrradianceSunCloudIndex] = separatedSunIrradianceClouds;
	StoreSkySHCamera(atmosphereIrradiance, skyCameraSH);
	StoreSkySHCloud(atmosphereIrradiance, skyCloudsSH);
}

