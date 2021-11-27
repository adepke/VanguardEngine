// Copyright (c) 2019-2021 Andrew Depke

#include "Base.hlsli"
#include "Atmosphere.hlsli"
#include "Camera.hlsli"
#include "Constants.hlsli"

#define RS \
	"RootFlags(0)," \
	"CBV(b0)," \
	"CBV(b1)," \
	"DescriptorTable(" \
		"SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"DescriptorTable(" \
		"SRV(t0, space = 1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
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
	float3 padding;
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

float3 ComputeDirection(uint3 dispatch)
{
	float2 uv = dispatch.xy / (float)IRRADIANCE_MAP_SIZE;
	uv = uv * 2.f - 1.f;
	switch (dispatch.z)
	{
		case 0: return float3(1.f, -uv.x, -uv.y);
		case 1: return float3(-1.f, uv.x, -uv.y);
		case 2: return float3(uv.x, 1.f, -uv.y);
		case 3: return float3(-uv.x, -1.f, -uv.y);
		case 4: return float3(uv.y, -uv.x, 1.f);
		case 5: return float3(-uv.y, -uv.x, -1.f);
	}
	
	return 0.f;
}

[RootSignature(RS)]
[numthreads(IRRADIANCE_MAP_SIZE, IRRADIANCE_MAP_SIZE, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{	
	float3 cameraPosition = camera.position.xyz / 1000.f;  // Atmosphere distances work in terms of kilometers due to floating point precision, so convert.
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 planetCenter = float3(0.f, 0.f, -bindData.atmosphere.radiusBottom);  // World origin is planet surface.
	
	RWTexture2DArray<float4> irradianceMap = textureArraysRW[bindData.convolutionTexture];
	
	float3 forward = normalize(ComputeDirection(dispatchId));
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