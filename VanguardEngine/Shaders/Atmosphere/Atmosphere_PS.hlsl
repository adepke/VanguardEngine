// Copyright (c) 2019-2021 Andrew Depke

#include "Atmosphere/Atmosphere_RS.hlsli"
#include "Base.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Camera.hlsli"

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
};

ConstantBuffer<AtmosphereBindData> bindData : register(b1);

struct Input
{
	float4 positionCS : SV_POSITION;
	float2 uv : UV;
};

struct Output
{
	float4 color : SV_Target;
};

[RootSignature(RS)]
Output main(Input input)
{
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	float3 planetCenter = float3(0.f, 0.f, -bindData.atmosphere.radiusBottom);  // World origin is planet surface.
	
	float3 rayDirection = ComputeRayDirection(camera, input.uv);
	
	Texture2D transmittanceLut = textures[bindData.transmissionTexture];
	Texture3D scatteringLut = textures3D[bindData.scatteringTexture];
	Texture2D irradianceLut = textures[bindData.irradianceTexture];
	
	float3 atmosphere = SampleAtmosphere(bindData.atmosphere, camera, rayDirection, sunDirection, planetCenter, transmittanceLut, scatteringLut, irradianceLut, lutSampler);
	
	Output output;
	output.color.rgb = atmosphere;
	output.color.a = 1.f;
	
	return output;
}