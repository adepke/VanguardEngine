// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Constants.hlsli"
#include "Camera.hlsli"
#include "CubeMap.hlsli"
#include "Clouds/Core.hlsli"

struct BindData
{
	uint luminanceTexture;
	uint environmentCloudsTexture;
	uint weatherTexture;
	uint baseShapeNoiseTexture;
	uint atmosphereIrradianceBuffer;
	uint cameraBuffer;
	uint cameraIndex;
	float solarZenithAngle;
	float2 wind;
	float time;
	float densityMultiplier;
	uint baseFace;
	uint jitterFrame;
	float blendFactor;
};

ConstantBuffer<BindData> bindData : register(b0);

// Low-detail raymarching that ignores geometry since we don't want to consider geometry for reflections here.
void RayMarch(Camera camera, float3 direction, float3 sunDirection, float2 wind, float time, float density, float jitter,
	out float3 scatteredLuminance, out float transmittance)
{
	scatteredLuminance = 0.xxx;
	transmittance = 1;

	Texture3D<float> baseShapeNoiseTexture = ResourceDescriptorHeap[bindData.baseShapeNoiseTexture];
	Texture3D<float> detailShapeNoiseTexture;  // Null texture (envmapping always marches at low detail).
	Texture3D<float4> curlNoiseTexture;  // Null texture (envmapping always marches at low detail).
	StructuredBuffer<float3> atmosphereIrradiance = ResourceDescriptorHeap[bindData.atmosphereIrradianceBuffer];
	Texture2D<float3> weatherTexture = ResourceDescriptorHeap[bindData.weatherTexture];

	const float planetRadius = 6360.0;  // #TODO: Get from atmosphere data.
	
	float3 origin = ComputeAtmosphereCameraPosition(camera);

	// #TODO: The march bounds setup is common to a few places, refactor into clouds core and re-use.
	float marchStart;
	float marchEnd;
	float gapStart = 0.f;
	float gapEnd = 0.f;

	float2 topBoundaryIntersect;
	if (RaySphereIntersection(origin, direction, planetCenter, planetRadius + cloudLayerTop, topBoundaryIntersect))
	{
		marchStart = max(topBoundaryIntersect.x, 0);
		marchEnd = topBoundaryIntersect.y;

		float2 bottomBoundaryIntersect;
		if (RaySphereIntersection(origin, direction, planetCenter, planetRadius + cloudLayerBottom, bottomBoundaryIntersect))
		{
			if (all(bottomBoundaryIntersect > 0))
			{
				gapStart = bottomBoundaryIntersect.x;
				gapEnd = bottomBoundaryIntersect.y;
			}
			else if (bottomBoundaryIntersect.y > 0)
			{
				marchStart = max(marchStart, bottomBoundaryIntersect.y);
			}
		}
	}
	else
	{
		// Outside of the cloud layer.
		return;
	}

	// Stop short if we hit the planet.
	float2 planetIntersect;
	if (RaySphereIntersection(origin, direction, planetCenter, planetRadius, planetIntersect))
	{
		marchEnd = min(marchEnd, planetIntersect.x);
	}

	marchStart = max(0, marchStart);
	marchEnd = max(0, marchEnd);

	if (marchEnd <= marchStart)
		return;

	// Precompute the noise kernel once for this pixel.
	ComputeNoiseKernel(sunDirection);

	float depth;  // Unused.
	RayMarchInternal(baseShapeNoiseTexture, detailShapeNoiseTexture, curlNoiseTexture, atmosphereIrradiance, weatherTexture,
		origin, direction, jitter, marchStart, marchEnd, gapStart, gapEnd, sunDirection, wind, time, density,
		scatteredLuminance, transmittance, depth);
}

// Temporally offset interleaved gradient noise in [0, 1).
// Spatially high-frequency to help reduce banding.
float InterleavedGradientNoise(float2 pixel, uint frame)
{
	pixel += 5.588238f * (float)(frame % 64);
	return frac(52.9829189f * frac(0.06711056f * pixel.x + 0.00583715f * pixel.y));
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture2DArray<float4> cloudMap = ResourceDescriptorHeap[bindData.environmentCloudsTexture];
	float width, height, elements;
	cloudMap.GetDimensions(width, height, elements);

	const uint face = bindData.baseFace + dispatchId.z;

	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	const float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));

	float2 uv = (float2(dispatchId.xy) + 0.5f) / width;
	uv = uv * 2.f - 1.f;
	const float3 direction = normalize(ComputeDirection(uv, face));

	const float jitter = InterleavedGradientNoise(float2(dispatchId.xy), bindData.jitterFrame);

	float3 scatteredLuminance;
	float transmittance;
	RayMarch(camera, direction, sunDirection, bindData.wind, bindData.time, bindData.densityMultiplier, jitter,
		scatteredLuminance, transmittance);

	const uint3 texel = uint3(dispatchId.xy, face);
	const float4 history = cloudMap[texel];
	cloudMap[texel] = lerp(history, float4(scatteredLuminance, transmittance), bindData.blendFactor);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void CompositeMain(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture2DArray<float4> luminanceMap = ResourceDescriptorHeap[bindData.luminanceTexture];
	float width, height, elements;
	luminanceMap.GetDimensions(width, height, elements);

	TextureCube<float4> cloudMap = ResourceDescriptorHeap[bindData.environmentCloudsTexture];

	float2 uv = (float2(dispatchId.xy) + 0.5f) / width;
	uv = uv * 2.f - 1.f;
	const float3 direction = normalize(ComputeDirection(uv, dispatchId.z));
	
	// Sample, since the cloud cube resolution might be different from the luminance cube.
	const float4 cloud = cloudMap.SampleLevel(bilinearClamp, direction, 0.f);

	const float3 sky = luminanceMap[dispatchId].rgb;
	luminanceMap[dispatchId] = float4(sky * cloud.a + cloud.rgb, 0.f);
}
