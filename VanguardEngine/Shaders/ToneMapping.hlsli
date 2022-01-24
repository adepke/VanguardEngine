// Copyright (c) 2019-2022 Andrew Depke

#ifndef __TONEMAPPING_HLSLI__
#define __TONEMAPPING_HLSLI__

float LinearToLuminance(float3 linearColor)
{
	return dot(linearColor, float3(0.2126, 0.7152, 0.0722));
}

float3 ReinhardLuminanceToneMap(float3 color, float maxLuminance)
{
	const float oldLuminance = LinearToLuminance(color);
	const float newLuminance = (oldLuminance * (1.0 + (oldLuminance / (maxLuminance * maxLuminance)))) / (1.0 + oldLuminance);

	return color * (newLuminance / oldLuminance);
}

static const float3x3 ACESInputMatrix = {
	{ 0.59719, 0.35458, 0.04823 },
	{ 0.07600, 0.90834, 0.01566 },
	{ 0.02840, 0.13383, 0.83777 }
};

static const float3x3 ACESOutputMatrix = {
	{ 1.60475, -0.53108, -0.07367 },
	{ -0.10208, 1.10813, -0.00605 },
	{ -0.00327, -0.07276, 1.07602 }
};

float3 ACESRRTODTFit(float3 color)
{
	const float3 a = color * (color + 0.0245786) - 0.000090537;
	const float3 b = color * (0.983729 * color + 0.4329510) + 0.238081;

	return a / b;
}

// ACES fit by Stephen Hill.
float3 ACESHillToneMap(float3 color)
{
	color = mul(ACESInputMatrix, color);
	color = ACESRRTODTFit(color);
	color = mul(ACESOutputMatrix, color);

	return saturate(color);
}

// ACES fit by Krzysztof Narkowicz.
float3 ACESNarkowiczToneMap(float3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;

	return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float3 ToneMap(float3 color)
{
	return ACESHillToneMap(color);
}

#endif  // __TONEMAPPING_HLSLI__