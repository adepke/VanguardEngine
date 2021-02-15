// Copyright (c) 2019-2021 Andrew Depke

#include "Default_RS.hlsli"
#include "Base.hlsli"
#include "Light.hlsli"

SamplerState defaultSampler : register(s0);

ConstantBuffer<MaterialData> material : register(b0);
ConstantBuffer<CameraData> cameraBuffer : register(b1);

// We can't use GetDimensions() on a root SRV, so instead pass the light count separately.
struct LightCount
{
    uint count;
};

ConstantBuffer<LightCount> lightCount : register(b2);
StructuredBuffer<Light> lights : register(t0, space1);

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
	
    Material materialSample;
    materialSample.baseColor = baseColor;
    materialSample.metalness = metallicRoughness.r;
    materialSample.roughness = metallicRoughness.g;
    materialSample.normal = normal;
    materialSample.occlusion = ambientOcclusion;
    materialSample.emissive = emissive;
	
    Camera camera;
    camera.position = cameraBuffer.position;
	
    for (uint i = 0; i < lightCount.count; ++i)
    {
        LightSample sample = SampleLight(lights[i], materialSample, camera, viewDirection, input.position, normalDirection);
        output.color.rgb += sample.diffuse.rgb;
    }

	// Ambient contribution.
	const float ambientLight = 0.025;
	float3 ambient = ambientLight * baseColor.rgb * ambientOcclusion;
	output.color.rgb += ambient;

	return output;
}