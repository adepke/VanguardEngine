// Copyright (c) 2019-2022 Andrew Depke

#ifndef __NOISE_HLSLI__
#define __NOISE_HLSLI__

// Noise ported from: https://github.com/sebh/TileableVolumeNoise

// HLSL and GLM/GLSL have different behavior in how they treat negative divisors with mod()/fmod(). Replicate the GLM behavior.
float Mod(float x, float y)
{
    return x - y * floor(x / y);
}

float2 Mod(float2 x, float2 y)
{
    return x - y * floor(x / y);
}

float3 Mod(float3 x, float3 y)
{
    return x - y * floor(x / y);
}

float4 Mod(float4 x, float4 y)
{
    return x - y * floor(x / y);
}

float Hash(float n)
{
	return frac(sin(n * 12.9898) * 43758.5453);
	//return frac(sin(n + 1.951) * 43758.5453);
}

float Noise(float3 x)
{
	float3 p = floor(x);
	float3 f = frac(x);

	f = f*f*(3.0.xxx - 2.0.xxx * f);
	float n = p.x + p.y*57.0f + 113.0f*p.z;
	return lerp(
		lerp(
			lerp(Hash(n + 0.0f), Hash(n + 1.0f), f.x),
			lerp(Hash(n + 57.0f), Hash(n + 58.0f), f.x),
			f.y),
		lerp(
			lerp(Hash(n + 113.0f), Hash(n + 114.0f), f.x),
			lerp(Hash(n + 170.0f), Hash(n + 171.0f), f.x),
			f.y),
		f.z);
}

float4 Mod289(float4 x)
{
	return x - floor(x * (1.0.xxxx / 289.0.xxxx)) * 289.0.xxxx;
}

float4 Permute(float4 x)
{
	return Mod289((x * 34.0 + 1.0) * x);
}

float4 TaylorInvSqrt(float4 x)
{
	return 1.79284291400159.xxxx - 0.85373472095314.xxxx * x;
}

float4 Fade(float4 x)
{
	return (x * x * x) * (x * (x * 6.0.xxxx - 15.0.xxxx) + 10.0.xxxx);
}

float Perlin(float4 p, float4 frequency)
{
    float4 Pi0 = Mod(floor(p), frequency);
    float4 Pi1 = Mod(Pi0 + 1.0, frequency);
	float4 Pf0 = frac(p);
	float4 Pf1 = Pf0 - 1.0;
	float4 ix = float4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
	float4 iy = float4(Pi0.y, Pi0.y, Pi1.y, Pi1.y);
	float4 iz0 = Pi0.zzzz;
	float4 iz1 = Pi1.zzzz;
	float4 iw0 = Pi0.wwww;
	float4 iw1 = Pi1.wwww;

	float4 ixy = Permute(Permute(ix) + iy);
	float4 ixy0 = Permute(ixy + iz0);
	float4 ixy1 = Permute(ixy + iz1);
	float4 ixy00 = Permute(ixy0 + iw0);
	float4 ixy01 = Permute(ixy0 + iw1);
	float4 ixy10 = Permute(ixy1 + iw0);
	float4 ixy11 = Permute(ixy1 + iw1);

	float4 gx00 = ixy00 / 7.0;
	float4 gy00 = floor(gx00) / 7.0;
	float4 gz00 = floor(gy00) / 6.0;
	gx00 = frac(gx00) - 0.5;
	gy00 = frac(gy00) - 0.5;
	gz00 = frac(gz00) - 0.5;
	float4 gw00 = 0.75.xxxx - abs(gx00) - abs(gy00) - abs(gz00);
	float4 sw00 = step(gw00, 0.xxxx);
	gx00 -= sw00 * (step(0.0, gx00) - 0.5);
	gy00 -= sw00 * (step(0.0, gy00) - 0.5);

	float4 gx01 = ixy01 / 7.0;
	float4 gy01 = floor(gx01) / 7.0;
	float4 gz01 = floor(gy01) / 6.0;
	gx01 = frac(gx01) - 0.5;
	gy01 = frac(gy01) - 0.5;
	gz01 = frac(gz01) - 0.5;
	float4 gw01 = 0.75.xxxx - abs(gx01) - abs(gy01) - abs(gz01);
	float4 sw01 = step(gw01, 0.xxxx);
	gx01 -= sw01 * (step(0.0, gx01) - 0.5);
	gy01 -= sw01 * (step(0.0, gy01) - 0.5);

	float4 gx10 = ixy10 / 7.0;
	float4 gy10 = floor(gx10) / 7.0;
	float4 gz10 = floor(gy10) / 6.0;
	gx10 = frac(gx10) - 0.5;
	gy10 = frac(gy10) - 0.5;
	gz10 = frac(gz10) - 0.5;
	float4 gw10 = 0.75.xxxx - abs(gx10) - abs(gy10) - abs(gz10);
	float4 sw10 = step(gw10, 0.xxxx);
	gx10 -= sw10 * (step(0.0, gx10) - 0.5);
	gy10 -= sw10 * (step(0.0, gy10) - 0.5);

	float4 gx11 = ixy11 / 7.0;
	float4 gy11 = floor(gx11) / 7.0;
	float4 gz11 = floor(gy11) / 6.0;
	gx11 = frac(gx11) - 0.5;
	gy11 = frac(gy11) - 0.5;
	gz11 = frac(gz11) - 0.5;
	float4 gw11 = 0.75.xxxx - abs(gx11) - abs(gy11) - abs(gz11);
	float4 sw11 = step(gw11, 0.xxxx);
	gx11 -= sw11 * (step(0.0, gx11) - 0.5);
	gy11 -= sw11 * (step(0.0, gy11) - 0.5);

	float4 g0000 = float4(gx00.x, gy00.x, gz00.x, gw00.x);
	float4 g1000 = float4(gx00.y, gy00.y, gz00.y, gw00.y);
	float4 g0100 = float4(gx00.z, gy00.z, gz00.z, gw00.z);
	float4 g1100 = float4(gx00.w, gy00.w, gz00.w, gw00.w);
	float4 g0010 = float4(gx10.x, gy10.x, gz10.x, gw10.x);
	float4 g1010 = float4(gx10.y, gy10.y, gz10.y, gw10.y);
	float4 g0110 = float4(gx10.z, gy10.z, gz10.z, gw10.z);
	float4 g1110 = float4(gx10.w, gy10.w, gz10.w, gw10.w);
	float4 g0001 = float4(gx01.x, gy01.x, gz01.x, gw01.x);
	float4 g1001 = float4(gx01.y, gy01.y, gz01.y, gw01.y);
	float4 g0101 = float4(gx01.z, gy01.z, gz01.z, gw01.z);
	float4 g1101 = float4(gx01.w, gy01.w, gz01.w, gw01.w);
	float4 g0011 = float4(gx11.x, gy11.x, gz11.x, gw11.x);
	float4 g1011 = float4(gx11.y, gy11.y, gz11.y, gw11.y);
	float4 g0111 = float4(gx11.z, gy11.z, gz11.z, gw11.z);
	float4 g1111 = float4(gx11.w, gy11.w, gz11.w, gw11.w);

	float4 norm00 = TaylorInvSqrt(float4(dot(g0000, g0000), dot(g0100, g0100), dot(g1000, g1000), dot(g1100, g1100)));
	g0000 *= norm00.x;
	g0100 *= norm00.y;
	g1000 *= norm00.z;
	g1100 *= norm00.w;

	float4 norm01 = TaylorInvSqrt(float4(dot(g0001, g0001), dot(g0101, g0101), dot(g1001, g1001), dot(g1101, g1101)));
	g0001 *= norm01.x;
	g0101 *= norm01.y;
	g1001 *= norm01.z;
	g1101 *= norm01.w;

	float4 norm10 = TaylorInvSqrt(float4(dot(g0010, g0010), dot(g0110, g0110), dot(g1010, g1010), dot(g1110, g1110)));
	g0010 *= norm10.x;
	g0110 *= norm10.y;
	g1010 *= norm10.z;
	g1110 *= norm10.w;

	float4 norm11 = TaylorInvSqrt(float4(dot(g0011, g0011), dot(g0111, g0111), dot(g1011, g1011), dot(g1111, g1111)));
	g0011 *= norm11.x;
	g0111 *= norm11.y;
	g1011 *= norm11.z;
	g1111 *= norm11.w;

	float n0000 = dot(g0000, Pf0);
	float n1000 = dot(g1000, float4(Pf1.x, Pf0.y, Pf0.z, Pf0.w));
	float n0100 = dot(g0100, float4(Pf0.x, Pf1.y, Pf0.z, Pf0.w));
	float n1100 = dot(g1100, float4(Pf1.x, Pf1.y, Pf0.z, Pf0.w));
	float n0010 = dot(g0010, float4(Pf0.x, Pf0.y, Pf1.z, Pf0.w));
	float n1010 = dot(g1010, float4(Pf1.x, Pf0.y, Pf1.z, Pf0.w));
	float n0110 = dot(g0110, float4(Pf0.x, Pf1.y, Pf1.z, Pf0.w));
	float n1110 = dot(g1110, float4(Pf1.x, Pf1.y, Pf1.z, Pf0.w));
	float n0001 = dot(g0001, float4(Pf0.x, Pf0.y, Pf0.z, Pf1.w));
	float n1001 = dot(g1001, float4(Pf1.x, Pf0.y, Pf0.z, Pf1.w));
	float n0101 = dot(g0101, float4(Pf0.x, Pf1.y, Pf0.z, Pf1.w));
	float n1101 = dot(g1101, float4(Pf1.x, Pf1.y, Pf0.z, Pf1.w));
	float n0011 = dot(g0011, float4(Pf0.x, Pf0.y, Pf1.z, Pf1.w));
	float n1011 = dot(g1011, float4(Pf1.x, Pf0.y, Pf1.z, Pf1.w));
	float n0111 = dot(g0111, float4(Pf0.x, Pf1.y, Pf1.z, Pf1.w));
	float n1111 = dot(g1111, Pf1);

	float4 fade_xyzw = Fade(Pf0);
	float4 n_0w = lerp(float4(n0000, n1000, n0100, n1100), float4(n0001, n1001, n0101, n1101), fade_xyzw.w);
	float4 n_1w = lerp(float4(n0010, n1010, n0110, n1110), float4(n0011, n1011, n0111, n1111), fade_xyzw.w);
	float4 n_zw = lerp(n_0w, n_1w, fade_xyzw.z);
	float2 n_yzw = lerp(float2(n_zw.x, n_zw.y), float2(n_zw.z, n_zw.w), fade_xyzw.y);
	float n_xyzw = lerp(n_yzw.x, n_yzw.y, fade_xyzw.x);
	return 2.2 * n_xyzw;
}

float PerlinNoise3D(float3 p, float frequency, float octaves)
{
	const float octaveFrenquencyFactor = 2;

	float sum = 0.0f;
	float weightSum = 0.0f;
	float weight = 0.5f;
	for (int oct = 0; oct < octaves; oct++)
	{
		float val = Perlin(float4(p * frequency, 0), frequency.xxxx);

		sum += val * weight;
		weightSum += weight;

		weight *= weight;
		frequency *= octaveFrenquencyFactor;
	}
	return saturate((sum / weightSum) * 0.5f + 0.5f);
}

float PerlinNoise2D(float2 p, float frequency, float octaves)
{
	return PerlinNoise3D(float3(p, 0.f), frequency, octaves);
}

float WorleyNoise3D(float3 p, float cellCount)
{
	const float3 pCell = p * cellCount;
	float d = 1.0e10;
	for (int xo = -1; xo <= 1; xo++)
	{
		for (int yo = -1; yo <= 1; yo++)
		{
			for (int zo = -1; zo <= 1; zo++)
			{
				float3 tp = floor(pCell) + float3(xo, yo, zo);

                tp = pCell - tp - Noise(Mod(tp, cellCount.xxx)).xxx;

				d = min(d, dot(tp, tp));
			}
		}
	}
	return saturate(d);
}

#endif  // __NOISE_HLSLI__