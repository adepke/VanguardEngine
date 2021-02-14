// Copyright (c) 2019-2021 Andrew Depke

#include "Default_RS.hlsli"
#include "Base.hlsli"
#include "Camera.hlsli"
#include "BRDF.hlsli"

SamplerState defaultSampler : register(s0);

struct Material
{
	uint baseColor;
	uint metallicRoughness;
	uint normal;
	uint occlusion;
	// Boundary
	uint emissive;
	float3 padding;
};

ConstantBuffer<Material> material : register(b0);
ConstantBuffer<CameraBuffer> cameraBuffer : register(b1);

struct PointLight
{
	float3 color;
	float padding0;
	// Boundary
	float3 position;
	float padding1;
};

ConstantBuffer<PointLight> pointLight : register(b2);

struct Input
{
	float4 positionSS : SV_POSITION;  // Screen space.
	float3 position : POSITION;  // World space.
	float3 normal : NORMAL;  // World space.
	float2 uv : UV;
	float3 tangent : TANGENT;  // World space.
	float3 bitangent : BITANGENT;  // World space.
};

struct Output
{
	float4 color : SV_Target;
};

[RootSignature(RS)]
Output main(Input input)
{
	Texture2D baseColorMap = textures[material.baseColor];

	float4 baseColor = baseColorMap.Sample(defaultSampler, input.uv);
	float2 metallicRoughness = { 0.0, 0.0 };
	float3 normal = input.normal;
	float ambientOcclusion = 1.0;
	float3 emissive = { 0.0, 0.0, 0.0 };

	// Alpha test.
	if (baseColor.a < alphaTestThreshold)
	{
		Output output;
		output.color = float4(0.0, 0.0, 0.0, 0.0);
		return output;
	}

	if (material.metallicRoughness > 0)
	{
		Texture2D metallicRoughnessMap = textures[material.metallicRoughness];
		metallicRoughness = metallicRoughnessMap.Sample(defaultSampler, input.uv).rg;
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

	Output output;
	output.color.rgb = float3(0.0, 0.0, 0.0);
	output.color.a = baseColor.a;

	float3 viewDirection = normalize(cameraBuffer.position - input.position);
	float3 normalDirection = normal;

	// For each light...

	float3 lightDirection = normalize(pointLight.position - input.position);
	float3 halfwayDirection = normalize(viewDirection + lightDirection);

	float distance = length(pointLight.position - input.position) * 0.002;
	float attenuation = 1.0 / (distance * distance);
	float3 radiance = pointLight.color * attenuation;

	float3 lightContribution = BRDF(normalDirection, viewDirection, halfwayDirection, lightDirection, baseColor.rgb, metallicRoughness.r, metallicRoughness.g, radiance);
	output.color.rgb += lightContribution;

	//

	// Ambient contribution.
	const float ambientLight = 0.025;
	float3 ambient = ambientLight * baseColor.rgb * ambientOcclusion;
	output.color.rgb += ambient;

	return output;
}