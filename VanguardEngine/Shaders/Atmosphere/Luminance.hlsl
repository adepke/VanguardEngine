// Copyright (c) 2019-2021 Andrew Depke

#include "Base.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Camera.hlsli"
#include "CubeMap.hlsli"

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
	uint luminanceTexture;
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

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	float3 cameraPosition = camera.position.xyz / 1000.f;  // Atmosphere distances work in terms of kilometers due to floating point precision, so convert.
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 planetCenter = float3(0.f, 0.f, -bindData.atmosphere.radiusBottom);  // World origin is planet surface.
	
	RWTexture2DArray<float4> luminanceMap = textureArraysRW[bindData.luminanceTexture];
	float width, height, depth;
	luminanceMap.GetDimensions(width, height, depth);
	
	float2 uv = (dispatchId.xy + 0.5f) / width;
	uv = uv * 2.f - 1.f;
	float3 direction = normalize(ComputeDirection(uv, dispatchId.z));
	
	float3 sample = SampleAtmosphere(cameraPosition, direction, sunDirection, planetCenter);
	luminanceMap[dispatchId] = float4(sample, 0.f);
}