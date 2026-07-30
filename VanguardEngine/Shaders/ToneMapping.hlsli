// Copyright (c) 2019-2022 Andrew Depke

#ifndef __TONEMAPPING_HLSLI__
#define __TONEMAPPING_HLSLI__

// Keep in sync with cvar "toneMapper"
#define TONEMAPPER_ACES_HILL 1
#define TONEMAPPER_ACES_NARKOWICZ 2
#define TONEMAPPER_AGX 3
#define TONEMAPPER_KHRONOS_PBR_NEUTRAL 4
#define TONEMAPPER_REINHARD 5

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

// Note these are transposed from reference for HLSL compatibility.
static const float3x3 AgXInsetMatrix = {
	{ 0.842479062253094,  0.0784335999999992, 0.0792237451477643 },
	{ 0.0423282422610123, 0.878468636469772,  0.0791661274605434 },
	{ 0.0423756549057051, 0.0784336000000000, 0.879142973793104  }
};

static const float3x3 AgXOutsetMatrix = {
	{  1.19687900512017,   -0.0980208811401368, -0.0990297440797205 },
	{ -0.0528968517574562,  1.15190312990417,   -0.0989611768448433 },
	{ -0.0529716355144438, -0.0980434501171241,  1.15107367264116   }
};

// Logarithmic encoding for the AgX inner sigmoid.
static const float AgXMinEv = -12.47393f;
static const float AgXMaxEv = 4.026069f;

float3 AgXDefaultContrastApprox(float3 x)
{
	// 6th-order polynomial approximation of the AgX sigmoid (sRGB output).
	float3 x2 = x * x;
	float3 x4 = x2 * x2;
	return + 15.5     * x4 * x2
	       - 40.14    * x4 * x
	       + 31.96    * x4
	       - 6.868    * x2 * x
	       + 0.4298   * x2
	       + 0.1191   * x
	       - 0.00232;
}

// AgX tone mapping by Troy Sobotka. Minimal implementation by Benjamin Wrensch.
// Preserves chroma in highlights significantly better than ACES.
// See: https://github.com/sobotka/AgX and https://iolite-engine.com/blog_posts/minimal_agx_implementation
float3 AgXToneMap(float3 color)
{
	// Avoid log(0).
	color = max(color, 1e-10);

	// 1. Apply the AgX inset (compresses the color volume into the renderable cone).
	color = mul(AgXInsetMatrix, color);

	// 2. Encode in log2 space, normalized to [0, 1] across the AgX dynamic range.
	color = clamp(log2(color), AgXMinEv, AgXMaxEv);
	color = (color - AgXMinEv) / (AgXMaxEv - AgXMinEv);

	// 3. Apply the AgX sigmoid.
	color = AgXDefaultContrastApprox(color);

	// Blender preset on top to reduce flatness.
	{
		const float3 slope = 1.0;
		const float3 offset = 0.0;
		const float3 power = 1.35;
		const float saturation = 1.4;

		const float luma = LinearToLuminance(color);
		color = pow(max(color * slope + offset, 0.0), power);
		color = luma + saturation * (color - luma);
	}

	// 4. Apply the outset (decompresses chroma slightly, adds saturation back).
	color = mul(AgXOutsetMatrix, color);

	// AgX outputs in sRGB transfer space, but our backbuffer is sRGB-formatted (UNORM_SRGB) and applies the
	// sRGB encoding for us. So we need to undo AgX's implicit sRGB encoding here by going to linear.
	color = pow(max(color, 0.0), 2.2);

	return saturate(color);
}

// Khronos PBR Neutral Tone Mapper.
// See: https://modelviewer.dev/examples/tone-mapping
float3 KhronosPBRNeutralToneMap(float3 color)
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression)
		return color;

	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return saturate(lerp(color, newPeak.xxx, g));
}

float3 ToneMap(float3 color, uint toneMapper)
{
	switch (toneMapper)
	{
	case TONEMAPPER_ACES_HILL: return ACESHillToneMap(color);
	case TONEMAPPER_ACES_NARKOWICZ: return ACESNarkowiczToneMap(color);
	case TONEMAPPER_AGX: return AgXToneMap(color);
	case TONEMAPPER_KHRONOS_PBR_NEUTRAL: return KhronosPBRNeutralToneMap(color);
	case TONEMAPPER_REINHARD: return ReinhardLuminanceToneMap(color, 100.0);
	default: return color;
	}
}

#endif  // __TONEMAPPING_HLSLI__