// Copyright (c) 2019-2021 Andrew Depke

#include "Base.hlsli"
#include "Atmosphere.hlsli"
#include "Camera.hlsli"
#include "Constants.hlsli"
#include "ImportanceSampling.hlsli"
#include "BRDF.hlsli"

#define RS \
	"RootFlags(0)," \
	"CBV(b0)," \
	"CBV(b1)," \
	"DescriptorTable(" \
		"SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"DescriptorTable(" \
		"SRV(t0, space = 1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"DescriptorTable(" \
		"UAV(u0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"DescriptorTable(" \
		"UAV(u0, space = 2, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"StaticSampler(" \
		"s0," \
		"filter = FILTER_MIN_MAG_MIP_LINEAR," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP)"

ConstantBuffer<Camera> camera : register(b0);
SamplerState lutSampler : register(s0);

struct AtmosphereBindData
{
	AtmosphereData atmosphere;
	// Boundary
	uint transmissionTexture;
	uint scatteringTexture;
	uint irradianceTexture;
	float solarZenithAngle;
	// Boundary
	uint convolutionTexture;
	uint brdfTexture;
	float2 padding;
	// Boundary
	uint prefilterMips[PREFILTER_LEVELS];
};

ConstantBuffer<AtmosphereBindData> bindData : register(b1);

float3 SampleAtmosphere(float3 cameraPosition, float3 direction, float3 sunDirection, float3 planetCenter)
{
	Texture2D transmittanceLut = textures[bindData.transmissionTexture];
	Texture3D scatteringLut = textures3D[bindData.scatteringTexture];
	Texture2D irradianceLut = textures[bindData.irradianceTexture];
	
	float3 transmittance;
	float3 radiance = GetSkyRadiance(bindData.atmosphere, transmittanceLut, scatteringLut, lutSampler, cameraPosition - planetCenter, direction, 0.f, sunDirection, transmittance);
	
	float4 planetRadiance = GetPlanetSurfaceRadiance(bindData.atmosphere, planetCenter, cameraPosition, direction, sunDirection, transmittanceLut, scatteringLut, irradianceLut, lutSampler);
	radiance = lerp(radiance, planetRadiance.xyz, planetRadiance.w);
	
	return 1.f - exp(-radiance * 10.f);
}

float3 ComputeDirection(float2 uv, uint z)
{	
	switch (z)
	{
		case 0: return float3(1.f, -uv.y, -uv.x);
		case 1: return float3(-1.f, uv.y, uv.x);
		case 2: return float3(uv.x, 1.f, uv.y);
		case 3: return float3(uv.x, -1.f, -uv.y);
		case 4: return float3(uv.x, uv.y, 1.f);
		case 5: return float3(-uv.x, -uv.y, -1.f);
	}
	
	return 0.f;
}

[RootSignature(RS)]
[numthreads(IRRADIANCE_MAP_SIZE, IRRADIANCE_MAP_SIZE, 1)]
void IrradianceMain(uint3 dispatchId : SV_DispatchThreadID)
{	
	float3 cameraPosition = camera.position.xyz / 1000.f;  // Atmosphere distances work in terms of kilometers due to floating point precision, so convert.
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 planetCenter = float3(0.f, 0.f, -bindData.atmosphere.radiusBottom);  // World origin is planet surface.
	
	RWTexture2DArray<float4> irradianceMap = textureArraysRW[bindData.convolutionTexture];
	
	float2 uv = (dispatchId.xy + 0.5f) / (float)IRRADIANCE_MAP_SIZE;
	uv = uv * 2.f - 1.f;
	
	float3 forward = normalize(ComputeDirection(uv, dispatchId.z));
	float3 up = float3(0.f, 0.f, 1.f);
	float3 left = normalize(cross(up, forward));
	up = normalize(cross(forward, left));
	
	// Intervals for numerical integration.
	static const int steps = 20;
	float3 irradianceSum = 0.f;
	
	// Traverse a hemisphere in spherical coordinates.
	for (int i = 0; i < steps; ++i)
	{
		float theta = 2.f * pi * ((float)i / float(steps - 1.f));
		for (int j = 0; j < steps; ++j)
		{
			float phi = 0.5f * pi * ((float)j / float(steps - 1.f));
			float3 tangentSpace = normalize(float3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi)));
			float3 worldSpace = tangentSpace.x * left + tangentSpace.y * up + tangentSpace.z * forward;
			float3 value = SampleAtmosphere(cameraPosition, worldSpace, sunDirection, planetCenter);
			irradianceSum += value * cos(phi) * sin(phi);  // Scaling improves light distribution.
		}
	}
	
	irradianceMap[dispatchId] = float4(pi * irradianceSum / (float)steps, 0.f);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void PrefilterMain(uint3 dispatchId : SV_DispatchThreadID)
{
	// Uses split sum approximation by Epic Games, see: https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
	
	float3 cameraPosition = camera.position.xyz / 1000.f;  // Atmosphere distances work in terms of kilometers due to floating point precision, so convert.
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 planetCenter = float3(0.f, 0.f, -bindData.atmosphere.radiusBottom);  // World origin is planet surface.
	
	RWTexture2DArray<float4> baseMip = textureArraysRW[bindData.prefilterMips[0]];
	float width, height, depth;
	baseMip.GetDimensions(width, height, depth);
	
	const uint maskShift = log2(width);
	const uint xyIndex = dispatchId.y * width + dispatchId.x;
	
	for (int i = 0; i < PREFILTER_LEVELS; ++i)
	{
		RWTexture2DArray<float4> prefilterMap = textureArraysRW[bindData.prefilterMips[i]];
		prefilterMap.GetDimensions(width, height, depth);
		
		uint groupMask = (1 << i) - 1;
		groupMask |= groupMask << maskShift;
		if ((xyIndex & groupMask) == 0)
		{
			const float roughness = (float)i / (PREFILTER_LEVELS - 1.f);
		
			uint2 pixel = dispatchId.xy / pow(2, i);
			float2 uv = (pixel + 0.5f) / width;
			uv = uv * 2.f - 1.f;
	
			// Simplified model.
			float3 normal = normalize(ComputeDirection(uv, dispatchId.z));
			float3 reflection = normal;
			float3 view = reflection;
		
			// Sample count, importance sampled.
			static const int steps = 1024;
			float sumWeight = 0.f;
			float3 sumSamples = 0.f;
	
			for (int j = 0; j < steps; ++j)
			{
				float2 sequence = HammersleySet(j, steps);
				float3 halfway = ImportanceSampledGGX(sequence, normal, roughness);
				float3 light = normalize(2.f * dot(view, halfway) * halfway - view);
				float normalDotLight = saturate(dot(normal, light));
				if (normalDotLight > 0.f)
				{
					// #TODO: Reduce aliasing artifacts from the predictible sequence. Can also jitter the sequence to improve visual fidelity.
					// See: https://chetanjags.wordpress.com/2015/08/26/image-based-lighting/
			
					sumWeight += normalDotLight;
					sumSamples += SampleAtmosphere(cameraPosition, light, sunDirection, planetCenter) * normalDotLight;
				}
			}
	
			prefilterMap[uint3(pixel, dispatchId.z)] = float4(sumSamples / sumWeight, 0.f);
		}
	}
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void BRDFMain(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture2D<float4> brdfLut = texturesRW[bindData.brdfTexture];
	float width, height;
	brdfLut.GetDimensions(width, height);
	
	float2 uv = (dispatchId.xy + 0.5f) / float2(width, height);
	float normalDotView = uv.x;
	float roughness = uv.y;
	
	float3 view = float3(sqrt(1.f - normalDotView * normalDotView), 0.f, normalDotView);
	float3 normal = float3(0.f, 0.f, 1.f);
	
	static const int steps = 1024;
	float a = 0.f;
	float b = 0.f;
	
	for (int i = 0; i < steps; ++i)
	{
		float2 sequence = HammersleySet(i, steps);
		float3 halfway = ImportanceSampledGGX(sequence, normal, roughness);
		float3 light = normalize(2.f * dot(view, halfway) * halfway - view);
		float normalDotLight = saturate(light.z);
		float normalDotHalfway = saturate(halfway.z);
		float viewDotHalfway = saturate(dot(view, halfway));
		if (normalDotLight > 0.f)
		{
			float g = SmithGeometry(normal, view, light, ComputeGeometryConstant(LightingType::IBL, roughness));
			float gVis = (g * viewDotHalfway) / (normalDotHalfway * normalDotView);
			float fresnel = FresnelSchlick(viewDotHalfway, 0.f);
			
			a += (1.f - fresnel) * gVis;
			b += fresnel * gVis;
		}
	}
	
	brdfLut[dispatchId.xy] = float4(a / (float)steps, b / (float)steps, 0.f, 0.f);
}