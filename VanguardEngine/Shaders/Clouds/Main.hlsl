// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Color.hlsli"
#include "Reprojection.hlsli"
#include "Clouds/Core.hlsli"

struct BindData
{
	uint weatherTexture;
	uint baseShapeNoiseTexture;
	uint detailShapeNoiseTexture;
	uint curlNoiseTexture;
	uint cameraBuffer;
	uint cameraIndex;
	float solarZenithAngle;
	uint frameIndex;
	uint depthTexture;
	uint geometryDepthTexture;  // Downsampled (min, max) reversed-Z bounds at output resolution.
	uint blueNoiseTexture;
	uint atmosphereIrradianceBuffer;
	uint2 outputResolution;
	uint2 upscaledResolution;
	float time;
	float2 wind;
	float densityMultiplier;
	float padding;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VertexIn
{
	uint vertexId : SV_VertexID;
};

struct PixelIn
{
	float4 positionCS : SV_POSITION;
	float2 uv : UV;
};

[RootSignature(RS)]
PixelIn VSMain(VertexIn input)
{
	PixelIn output;
	output.uv = float2((input.vertexId << 1) & 2, input.vertexId & 2);
	output.positionCS = float4((output.uv.x - 0.5) * 2.0, -(output.uv.y - 0.5) * 2.0, 0, 1);  // Z of 0 due to the inverse depth.

	return output;
}

[RootSignature(RS)]
#ifdef CLOUDS_ONLY_DEPTH
float PSMain(PixelIn input) : SV_Target
#else
float4 PSMain(PixelIn input) : SV_Target
#endif
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	Texture2D<float> blueNoiseTexture = ResourceDescriptorHeap[bindData.blueNoiseTexture];

#ifdef CLOUDS_STOCHASTIC
	// No subpixel jitter: rays go through pixel centers. Jittering the ray direction only adds
	// information when the history can represent subpixel detail (TAA: full res history plus
	// jitter-compensated reprojection). Here the accumulation history is the same resolution as the
	// samples, so direction jitter is pure variance - the EMA blends laterally shifted copies of the
	// image, which is by construction blur (averaging across the jitter footprint) plus edge shimmer
	// (the residual per-frame shift). The stochastic element lives entirely in the ray start offset,
	// which dithers banding along the ray without moving the image laterally.
	float2 jitteredUv = input.uv;
#else
	// Get the UV coordinates that are top-left aligned.
	float2 alignedUv = floor(input.uv * bindData.outputResolution) / float2(bindData.outputResolution);

	// Jitter the UV coordinates for temporal accumulation. This is used to offset the raymarch, but not the output
	// coordinates, which would be wrong then. Note that the jitter uses the upscaled resolution, not the low resolution.
	float2 jitteredUv = JitterUv(alignedUv, bindData.upscaledResolution, bindData.frameIndex);
#endif

	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
#ifdef CLOUDS_RENDER_ORTHOGRAPHIC
	// This is equivalent to -sunDirection.
	float3 rayDirection = ComputeRayDirection(camera, 0.5.xx);
#else
	float3 rayDirection = ComputeRayDirection(camera, jitteredUv);
#endif

	Texture3D<float> baseShapeNoiseTexture = ResourceDescriptorHeap[bindData.baseShapeNoiseTexture];
	Texture3D<float> detailShapeNoiseTexture = ResourceDescriptorHeap[bindData.detailShapeNoiseTexture];
	Texture3D<float4> curlNoiseTexture = ResourceDescriptorHeap[bindData.curlNoiseTexture];
	StructuredBuffer<float3> atmosphereIrradiance = ResourceDescriptorHeap[bindData.atmosphereIrradianceBuffer];
	Texture2D<float3> weatherTexture = ResourceDescriptorHeap[bindData.weatherTexture];
	Texture2D<float2> geometryDepthMinMaxTexture = ResourceDescriptorHeap[bindData.geometryDepthTexture];

	float3 scatteredLuminance;
	float transmittance;
	float depth;  // Kilometers.
#if defined(CLOUDS_DEBUG_MARCHCOUNT)
	int stepCount = RayMarchClouds(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture,
		geometryDepthMinMaxTexture, blueNoiseTexture, camera, input.uv, jitteredUv, bindData.outputResolution, bindData.frameIndex, rayDirection,
		sunDirection, bindData.wind, bindData.time, bindData.densityMultiplier, scatteredLuminance, transmittance, depth);
#elif defined(CLOUDS_DEBUG_NORMALVECTOR)
	float3 normal = RayMarchClouds(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture,
		geometryDepthMinMaxTexture, blueNoiseTexture, camera, input.uv, jitteredUv, bindData.outputResolution, bindData.frameIndex, rayDirection,
		sunDirection, bindData.wind, bindData.time, bindData.densityMultiplier, scatteredLuminance, transmittance, depth);
#else
	RayMarchClouds(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture,
		geometryDepthMinMaxTexture, blueNoiseTexture, camera, input.uv, jitteredUv, bindData.outputResolution, bindData.frameIndex, rayDirection,
		sunDirection, bindData.wind, bindData.time, bindData.densityMultiplier, scatteredLuminance, transmittance, depth);
#endif

	// NaN firewall: rare degenerate math in the march can emit NaNs. Never let them leave this pass,
	// they contaminate the temporal history and get smeared into large black blocks by the bloom chain.
	if (any(isnan(scatteredLuminance)) || isnan(transmittance) || isnan(depth))
	{
		scatteredLuminance = 0.xxx;
		transmittance = 1.f;
		depth = 1000000.f;
	}

#ifdef CLOUDS_ONLY_DEPTH
	return depth;
#else
	RWTexture2D<float> depthTexture = ResourceDescriptorHeap[bindData.depthTexture];
	depthTexture[input.uv * bindData.outputResolution] = depth;

	float4 output;
#if defined(CLOUDS_DEBUG_MARCHCOUNT)
	output = float4(MapToRainbow(stepCount / 200.f), 0.f);
#elif defined(CLOUDS_DEBUG_NORMALVECTOR)
	// Remap from [-1, 1] to [0, 1]
	output = float4(normal * 0.5f + 0.5f, 0.f);
#else
	output.rgb = scatteredLuminance;
	output.a = transmittance;
#endif

	return output;
#endif
}