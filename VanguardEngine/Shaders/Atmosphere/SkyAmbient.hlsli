// Copyright (c) 2019-2022 Andrew Depke

#ifndef __SKYAMBIENT_HLSLI__
#define __SKYAMBIENT_HLSLI__

#include "Utils/SH.hlsli"

// Utilities for working with the spherical harmonic projection of the sky.
// Two SH probes are captured, the first is at the camera position, the second in the cloud layer.

static const uint kAtmosphereIrradianceSunCameraIndex    = 0;
static const uint kAtmosphereIrradianceSunCloudIndex     = 1;
static const uint kAtmosphereIrradianceSkySHCameraBase   = 2;
static const uint kAtmosphereIrradianceSkySHCloudBase    = 11;
static const uint kAtmosphereIrradianceSkySHCoefficients = 9;
static const uint kAtmosphereIrradianceBufferEntries     = 20;

float3 LoadSunIrradianceCamera(StructuredBuffer<float3> atmosphereIrradiance)
{
	return atmosphereIrradiance[kAtmosphereIrradianceSunCameraIndex];
}

float3 LoadSunIrradianceCloud(StructuredBuffer<float3> atmosphereIrradiance)
{
	return atmosphereIrradiance[kAtmosphereIrradianceSunCloudIndex];
}

SH::L2_RGB LoadSkySHCamera(StructuredBuffer<float3> atmosphereIrradiance)
{
	SH::L2_RGB sh;
	[unroll]
	for (uint i = 0; i < kAtmosphereIrradianceSkySHCoefficients; ++i)
		sh.C[i] = atmosphereIrradiance[kAtmosphereIrradianceSkySHCameraBase + i];
	return sh;
}

SH::L2_RGB LoadSkySHCloud(StructuredBuffer<float3> atmosphereIrradiance)
{
	SH::L2_RGB sh;
	[unroll]
	for (uint i = 0; i < kAtmosphereIrradianceSkySHCoefficients; ++i)
		sh.C[i] = atmosphereIrradiance[kAtmosphereIrradianceSkySHCloudBase + i];
	return sh;
}

void StoreSkySHCamera(RWStructuredBuffer<float3> atmosphereIrradiance, SH::L2_RGB sh)
{
	[unroll]
	for (uint i = 0; i < kAtmosphereIrradianceSkySHCoefficients; ++i)
		atmosphereIrradiance[kAtmosphereIrradianceSkySHCameraBase + i] = sh.C[i];
}

void StoreSkySHCloud(RWStructuredBuffer<float3> atmosphereIrradiance, SH::L2_RGB sh)
{
	[unroll]
	for (uint i = 0; i < kAtmosphereIrradianceSkySHCoefficients; ++i)
		atmosphereIrradiance[kAtmosphereIrradianceSkySHCloudBase + i] = sh.C[i];
}

float3 SkySHAverageRadiance(SH::L2_RGB sh)
{
	return sh.C[0] * (1.0 / (2.0 * SH::SqrtPi));
}

#endif  // __SKYAMBIENT_HLSLI__
