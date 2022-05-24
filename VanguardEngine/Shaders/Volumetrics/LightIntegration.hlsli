// Copyright (c) 2019-2022 Andrew Depke

#ifndef __LIGHTINTEGRATION_HLSLI__
#define __LIGHTINTEGRATION_HLSLI__

// More energy-conserving approximation from Frostbite.
// See: http://advances.realtimerendering.com/s2015/Frostbite%20PB%20and%20unified%20volumetrics.pptx
void ComputeScatteringIntegration(float density, float3 luminance, float dist, float3 scatteringCoeff,
	float3 absorptionCoeff, inout float3 scatteredLuminance, inout float3 transmittance)
{
	const float3 extinctionCoeff = scatteringCoeff + absorptionCoeff;
	const float3 extinction = max(extinctionCoeff * density, 0.0000001.xxx);
	const float3 transmittanceSample = exp(-extinction * dist);

	luminance *= scatteringCoeff;
	const float3 integScatt = (luminance - luminance * transmittanceSample) / extinction;

	scatteredLuminance += transmittance * integScatt;
	transmittance *= transmittanceSample;
}

#endif  // __LIGHTINTEGRATION_HLSLI__