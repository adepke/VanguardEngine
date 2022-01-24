// Copyright (c) 2019-2022 Andrew Depke

#ifndef __COLOR_HLSLI__
#define __COLOR_HLSLI__

// Color space conversions credit:
// https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
// Real-time Rendering 4th Edition.

float3 LinearToSRGB(float3 linearColor)
{
	const float3 s1 = sqrt(linearColor);
	const float3 s2 = sqrt(s1);
	const float3 s3 = sqrt(s2);
	return 0.662002687f * s1 + 0.684122060f * s2 - 0.323583601f * s3 - 0.0225411470f * linearColor;
}

float3 SRGBToLinear(float3 sRGBColor)
{
	return sRGBColor * (sRGBColor * (sRGBColor * 0.305306011f + 0.682171111f) + 0.012522878f);
}

// Only for sRGB and Rec. 709 color space.
float LinearToLuminance(float3 linearColor)
{
	return dot(linearColor, float3(0.2126f, 0.7152f, 0.0722f));  // CIE transport function.
}

#endif  // __COLOR_HLSLI__