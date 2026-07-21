// Copyright (c) 2019-2022 Andrew Depke

#ifndef __FILTERING_HLSLI__
#define __FILTERING_HLSLI__

// Requires RootSignature.hlsli to be included first for the bilinearClamp sampler.

// Catmull-Rom bicubic resampling, 9 taps via bilinear fetches (Jimenez). Bilinear resampling acts as
// a strong low pass - repeated every frame on temporal history it compounds into visible blur, and as
// a single 4x spatial upscale it softens heavily. Catmull-Rom preserves sharpness and degrades to a
// near-identity filter when sampling at texel centers. Note the negative lobes can overshoot: clamp
// or clip the result if the signal must stay in a physical range.
float4 SampleCatmullRom(Texture2D<float4> source, float2 uv, float2 resolution)
{
	const float2 samplePos = uv * resolution;
	const float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;
	const float2 f = samplePos - texPos1;

	const float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
	const float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
	const float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
	const float2 w3 = f * f * (-0.5f + 0.5f * f);

	const float2 w12 = w1 + w2;
	const float2 offset12 = w2 / w12;

	const float2 texPos0 = (texPos1 - 1.f) / resolution;
	const float2 texPos3 = (texPos1 + 2.f) / resolution;
	const float2 texPos12 = (texPos1 + offset12) / resolution;

	float4 result = 0.xxxx;
	result += source.SampleLevel(bilinearClamp, float2(texPos0.x, texPos0.y), 0) * w0.x * w0.y;
	result += source.SampleLevel(bilinearClamp, float2(texPos12.x, texPos0.y), 0) * w12.x * w0.y;
	result += source.SampleLevel(bilinearClamp, float2(texPos3.x, texPos0.y), 0) * w3.x * w0.y;
	result += source.SampleLevel(bilinearClamp, float2(texPos0.x, texPos12.y), 0) * w0.x * w12.y;
	result += source.SampleLevel(bilinearClamp, float2(texPos12.x, texPos12.y), 0) * w12.x * w12.y;
	result += source.SampleLevel(bilinearClamp, float2(texPos3.x, texPos12.y), 0) * w3.x * w12.y;
	result += source.SampleLevel(bilinearClamp, float2(texPos0.x, texPos3.y), 0) * w0.x * w3.y;
	result += source.SampleLevel(bilinearClamp, float2(texPos12.x, texPos3.y), 0) * w12.x * w3.y;
	result += source.SampleLevel(bilinearClamp, float2(texPos3.x, texPos3.y), 0) * w3.x * w3.y;
	return result;
}

#endif  // __FILTERING_HLSLI__
