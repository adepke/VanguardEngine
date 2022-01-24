// Copyright (c) 2019-2022 Andrew Depke

#include "Default_RS.hlsli"
#include "Base.hlsli"
#include "Camera.hlsli"
#include "Material.hlsli"
#include "Light.hlsli"
#include "Clusters/Clusters.hlsli"
#include "IBL/ImageBasedLighting.hlsli"

SamplerState defaultSampler : register(s0);

struct ClusterData
{
	uint3 dimensions;
	float logY;
};

ConstantBuffer<ClusterData> clusterData : register(b0);
ConstantBuffer<MaterialData> material : register(b1);
ConstantBuffer<Camera> camera : register(b2);
StructuredBuffer<Light> lights : register(t0, space1);
StructuredBuffer<uint> clusteredLightList : register(t1, space1);
StructuredBuffer<uint2> clusteredLightInfo : register(t2, space1);

struct IblData
{
	uint irradianceTexture;
	uint prefilterTexture;
	uint brdfTexture;
	uint prefilterLevels;
};

ConstantBuffer<IblData> iblData : register(b3);
StructuredBuffer<float3> sunTransmittance : register(t3, space1);

struct Input
{
	float4 positionSS : SV_POSITION;  // Screen space.
	float3 position : POSITION;  // World space.
	float3 normal : NORMAL;  // World space.
	float2 uv : UV;
	float3 tangent : TANGENT;  // World space.
	float3 bitangent : BITANGENT;  // World space.
	float depthVS : DEPTH;  // View space.
	float4 color : COLOR;
};

struct Output
{
	float4 color : SV_Target;
};

[RootSignature(RS)]
Output main(Input input)
{
	float4 baseColor = input.color;
	
	if (material.baseColor > 0)
	{
		Texture2D baseColorMap = textures[material.baseColor];
		baseColor = baseColorMap.Sample(defaultSampler, input.uv);
		
		clip(baseColor.a < alphaTestThreshold ? -1 : 1);
	}
	
	float2 metallicRoughness = { 1.0, 1.0 };
	float3 normal = input.normal;
	float ambientOcclusion = 1.0;
	float3 emissive = { 1.0, 1.0, 1.0 };

	if (material.metallicRoughness > 0)
	{
		Texture2D metallicRoughnessMap = textures[material.metallicRoughness];
		metallicRoughness = metallicRoughnessMap.Sample(defaultSampler, input.uv).bg;  // GLTF 2.0 spec.
	}

	if (material.normal > 0)
	{
		// Construct the TBN matrix.
		float3x3 TBN = float3x3(input.tangent, input.bitangent, input.normal);

		Texture2D normalMap = textures[material.normal];
		normal = normalMap.Sample(defaultSampler, input.uv).rgb;
		normal = normal * 2.0 - 1.0;  // Remap from [0, 1] to [-1, 1].
		normal = normalize(mul(normal, TBN));  // Convert the normal vector from tangent space to world space.
	}

	if (material.occlusion > 0)
	{
		Texture2D occlusionMap = textures[material.occlusion];
		ambientOcclusion = occlusionMap.Sample(defaultSampler, input.uv).r;
	}

	if (material.emissive > 0)
	{
		Texture2D emissiveMap = textures[material.emissive];
		emissive = emissiveMap.Sample(defaultSampler, input.uv).rgb;
	}
	
	baseColor *= material.baseColorFactor;
	metallicRoughness *= float2(material.metallicFactor, material.roughnessFactor);
	emissive *= material.emissiveFactor;

	Output output;
	output.color.rgb = float3(0.0, 0.0, 0.0);
	output.color.a = baseColor.a;

	float3 viewDirection = normalize(camera.position.xyz - input.position);
	float3 normalDirection = normal;
	
	Material materialSample;
	materialSample.baseColor = baseColor;
	materialSample.metalness = metallicRoughness.r;
	materialSample.roughness = metallicRoughness.g * metallicRoughness.g;  // Perceptually linear roughness remapping, from observations by Disney.
	materialSample.normal = normal;
	materialSample.occlusion = ambientOcclusion;
	materialSample.emissive = emissive;
	
	uint3 clusterId = DrawToClusterId(FROXEL_SIZE, clusterData.logY, camera, input.positionSS.xy, input.depthVS);
	uint2 lightInfo = clusteredLightInfo[ClusterId2Index(clusterData.dimensions, clusterId)];
	for (uint i = 0; i < lightInfo.y; ++i)
	{
		uint lightIndex = clusteredLightList[lightInfo.x + i];
		Light light = lights[lightIndex];
		// The sun needs to have atmospheric transmittance applied.
		// This is a very hacky solution.
		if (light.type == LightType::Directional)
		{
			light.color *= sunTransmittance[0];
		}
		
		LightSample sample = SampleLight(light, materialSample, camera, viewDirection, input.position, normalDirection);
		output.color.rgb += sample.diffuse.rgb;
	}
	
	TextureCube irradianceMap = textureCubes[iblData.irradianceTexture];
	TextureCube prefilterMap = textureCubes[iblData.prefilterTexture];
	Texture2D brdfMap = textures[iblData.brdfTexture];
	
	float width, height, prefilterMipCount;
	prefilterMap.GetDimensions(0, width, height, prefilterMipCount);
	
	float3 ibl = ComputeIBL(normalDirection, viewDirection, materialSample, iblData.prefilterLevels, irradianceMap, prefilterMap, brdfMap, defaultSampler);
	output.color.rgb += ibl;
	
	output.color.rgb += materialSample.emissive;
	
	return output;
}