// Copyright (c) 2019-2021 Andrew Depke

// Color space conversions credit: https://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html

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