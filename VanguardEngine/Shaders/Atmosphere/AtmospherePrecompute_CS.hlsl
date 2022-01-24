// Copyright (c) 2019-2022 Andrew Depke

#include "Atmosphere/AtmospherePrecompute_RS.hlsli"
#include "Base.hlsli"
#include "Atmosphere/Atmosphere.hlsli"

struct PrecomputeData
{
	AtmosphereData atmosphere;
	// Boundary
	uint transmissionTexture;
	uint scatteringTexture;
	uint irradianceTexture;
	uint deltaRayleighTexture;
	// Boundary
	uint deltaMieTexture;
	uint deltaScatteringDensityTexture;
	uint deltaIrradianceTexture;
	int scatteringOrder;
};

ConstantBuffer<PrecomputeData> precomputeData : register(b0);

SamplerState lutSampler : register(s0);

struct Input
{
	uint groupIndex : SV_GroupIndex;
	uint3 dispatchId : SV_DispatchThreadID;
};

float2 DispatchToLutCoords(int3 dispatchId, Texture2D lut, out float2 dimensions)
{
	lut.GetDimensions(dimensions.x, dimensions.y);
	return (dispatchId.xy + 0.5f) / dimensions;
}

float2 DispatchToLutCoords(int3 dispatchId, RWTexture2D<float4> lut, out float2 dimensions)
{
	lut.GetDimensions(dimensions.x, dimensions.y);
	return (dispatchId.xy + 0.5f) / dimensions;
}

float3 DispatchToLutCoords(int3 dispatchId, Texture3D lut, out float3 dimensions)
{
	lut.GetDimensions(dimensions.x, dimensions.y, dimensions.z);
	return (dispatchId.xyz + 0.5f) / dimensions;
}

float3 DispatchToLutCoords(int3 dispatchId, RWTexture3D<float4> lut, out float3 dimensions)
{
	lut.GetDimensions(dimensions.x, dimensions.y, dimensions.z);
	return (dispatchId.xyz + 0.5f) / dimensions;
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void TransmittanceLutMain(Input input)
{
	RWTexture2D<float4> transmittance = texturesRW[precomputeData.transmissionTexture];
	
	float2 dimensions;
	float2 uv = DispatchToLutCoords(input.dispatchId, transmittance, dimensions);
	
	float3 transmittanceData = ComputeTransmittanceToAtmosphereTopLut(precomputeData.atmosphere, uv, dimensions);
	transmittance[input.dispatchId.xy] = float4(transmittanceData, 1.f);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void DirectIrradianceLutMain(Input input)
{
	Texture2D<float4> transmittance = textures[precomputeData.transmissionTexture];
	RWTexture2D<float4> deltaIrradiance = texturesRW[precomputeData.deltaIrradianceTexture];
	RWTexture2D<float4> irradiance = texturesRW[precomputeData.irradianceTexture];
	
	float2 dimensions;
	float2 uv = DispatchToLutCoords(input.dispatchId, irradiance, dimensions);
	
	deltaIrradiance[input.dispatchId.xy] = float4(ComputeDirectIrradianceLut(precomputeData.atmosphere, transmittance, lutSampler, uv, dimensions), 0.f);
	irradiance[input.dispatchId.xy] = float4(0.f, 0.f, 0.f, 0.f);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void IndirectIrradianceLutMain(Input input)
{
	Texture3D<float4> singleRayleighScattering = textures3D[precomputeData.deltaRayleighTexture];
	Texture3D<float4> singleMieScattering = textures3D[precomputeData.deltaMieTexture];
	Texture3D<float4> multipleScattering = textures3D[precomputeData.deltaRayleighTexture];
	RWTexture2D<float4> deltaIrradiance = texturesRW[precomputeData.deltaIrradianceTexture];
	RWTexture2D<float4> irradiance = texturesRW[precomputeData.irradianceTexture];
	
	float2 dimensions;
	float2 uv = DispatchToLutCoords(input.dispatchId, irradiance, dimensions);
	
	float3 indirectIrradiance = ComputeIndirectIrradianceLut(precomputeData.atmosphere, singleRayleighScattering, singleMieScattering, multipleScattering, lutSampler, uv, precomputeData.scatteringOrder, dimensions);
	deltaIrradiance[input.dispatchId.xy] = float4(indirectIrradiance, 0.f);
	irradiance[input.dispatchId.xy] += float4(indirectIrradiance, 0.f);  // Accumulate irradiance.
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void SingleScatteringLutMain(Input input)
{
	Texture2D<float4> transmittance = textures[precomputeData.transmissionTexture];
	RWTexture3D<float4> deltaRayleigh = textures3DRW[precomputeData.deltaRayleighTexture];
	RWTexture3D<float4> deltaMie = textures3DRW[precomputeData.deltaMieTexture];
	RWTexture3D<float4> scattering = textures3DRW[precomputeData.scatteringTexture];
	
	float3 dimensions;
	float3 uvw = DispatchToLutCoords(input.dispatchId, scattering, dimensions);
	
	float3 rayleigh;
	float3 mie;
	ComputeSingleScatteringLut(precomputeData.atmosphere, transmittance, lutSampler, uvw, dimensions, rayleigh, mie);
	deltaRayleigh[input.dispatchId.xyz] = float4(rayleigh, 0.f);
	deltaMie[input.dispatchId.xyz] = float4(mie, 0.f);
	scattering[input.dispatchId.xyz] = float4(rayleigh, mie.r);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void ScatteringDensityLutMain(Input input)
{
	Texture2D<float4> transmittance = textures[precomputeData.transmissionTexture];
	Texture3D<float4> singleRayleighScattering = textures3D[precomputeData.deltaRayleighTexture];
	Texture3D<float4> singleMieScattering = textures3D[precomputeData.deltaMieTexture];
	Texture3D<float4> multipleScattering = textures3D[precomputeData.deltaRayleighTexture];
	Texture2D<float4> irradiance = textures[precomputeData.deltaIrradianceTexture];
	RWTexture3D<float4> scatteringDensity = textures3DRW[precomputeData.deltaScatteringDensityTexture];
	
	float3 dimensions;
	float3 uvw = DispatchToLutCoords(input.dispatchId, scatteringDensity, dimensions);
	
	scatteringDensity[input.dispatchId.xyz] = float4(ComputeScatteringDensityLut(precomputeData.atmosphere, transmittance, singleRayleighScattering, singleMieScattering, multipleScattering, irradiance, lutSampler, uvw, precomputeData.scatteringOrder), 0.f);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void MultipleScatteringLutMain(Input input)
{
	Texture2D<float4> transmittance = textures[precomputeData.transmissionTexture];
	Texture3D<float4> scatteringDensity = textures3D[precomputeData.deltaScatteringDensityTexture];
	RWTexture3D<float4> deltaMultipleScattering = textures3DRW[precomputeData.deltaRayleighTexture];
	RWTexture3D<float4> scattering = textures3DRW[precomputeData.scatteringTexture];
	
	float3 dimensions;
	float3 uvw = DispatchToLutCoords(input.dispatchId, scattering, dimensions);
	
	float4 multipleScattering = ComputeMultipleScatteringLut(precomputeData.atmosphere, transmittance, scatteringDensity, lutSampler, uvw);
	deltaMultipleScattering[input.dispatchId.xyz] = float4(multipleScattering.xyz, 0.f);
	scattering[input.dispatchId.xyz] += float4(multipleScattering.xyz / RayleighPhase(multipleScattering.w), 0.f);  // Accumulate scattering.
}