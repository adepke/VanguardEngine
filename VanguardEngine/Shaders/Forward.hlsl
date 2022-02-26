// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"
#include "Material.hlsli"
#include "Light.hlsli"
#include "Clusters/Clusters.hlsli"
#include "IBL/ImageBasedLighting.hlsli"

struct ClusterData
{
	uint lightListBuffer;
	uint lightInfoBuffer;
	float logY;
	float padding1;
	uint3 dimensions;
	float padding2;
};

struct IblData
{
	uint irradianceTexture;
	uint prefilterTexture;
	uint brdfTexture;
	uint prefilterLevels;
};

struct BindData
{
	uint objectBuffer;
	uint objectIndex;
	uint cameraBuffer;
	uint cameraIndex;
	VertexAssemblyData vertexAssemblyData;
	uint materialBuffer;  // #TODO: Replace with material buffer index.
	uint lightBuffer;
	uint sunTransmittanceBuffer;
	float padding;
	ClusterData clusterData;
	IblData iblData;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VertexIn
{
	uint vertexID : SV_VertexID;
};

struct PixelIn
{
	float4 positionCS : SV_POSITION;  // Clip space in VS, screen space in PS.
	float3 position : POSITION;  // World space.
	float3 normal : NORMAL;  // World space.
	float2 uv : UV;
	float3 tangent : TANGENT;  // World space.
	float3 bitangent : BITANGENT;  // World space.
	float depthVS : DEPTH;  // View space.
	float4 color : COLOR;
};

[RootSignature(RS)]
PixelIn VSMain(VertexIn input)
{
	StructuredBuffer<PerObject> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	PerObject perObject = objectBuffer[bindData.objectIndex];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	
	float4 position = LoadVertexPosition(bindData.vertexAssemblyData, input.vertexID);
	float4 normal = float4(LoadVertexNormal(bindData.vertexAssemblyData, input.vertexID), 0.f);
	float2 uv = LoadVertexTexcoord(bindData.vertexAssemblyData, input.vertexID);
	float4 tangent = LoadVertexTangent(bindData.vertexAssemblyData, input.vertexID);
	float4 bitangent = LoadVertexBitangent(bindData.vertexAssemblyData, input.vertexID);
	float4 color = LoadVertexColor(bindData.vertexAssemblyData, input.vertexID);
	
	PixelIn output;
	output.positionCS = position;
	output.positionCS = mul(output.positionCS, perObject.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.depthVS = output.positionCS.z;
	output.positionCS = mul(output.positionCS, camera.projection);
	output.position = mul(position, perObject.worldMatrix).xyz;
	output.normal = normalize(mul(normal, perObject.worldMatrix)).xyz;
	output.uv = uv;
	output.tangent = normalize(mul(tangent, perObject.worldMatrix)).xyz;
	output.bitangent = normalize(mul(bitangent, perObject.worldMatrix)).xyz;
	output.color = color;
	
	return output;
}

[RootSignature(RS)]
float4 PSMain(PixelIn input) : SV_Target
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	StructuredBuffer<MaterialData> materialBuffer = ResourceDescriptorHeap[bindData.materialBuffer];
	MaterialData material = materialBuffer[0];  // #TODO: Merge all materials into a single buffer.
	
	float4 baseColor = input.color;
	
	if (material.baseColor > 0)
	{
		Texture2D<float4> baseColorMap = ResourceDescriptorHeap[material.baseColor];
		baseColor = baseColorMap.Sample(anisotropicWrap, input.uv);
		
		clip(baseColor.a < alphaTestThreshold ? -1 : 1);
	}
	
	float2 metallicRoughness = { 1.0, 1.0 };
	float3 normal = input.normal;
	float ambientOcclusion = 1.0;
	float3 emissive = { 1.0, 1.0, 1.0 };

	if (material.metallicRoughness > 0)
	{
		Texture2D<float4> metallicRoughnessMap = ResourceDescriptorHeap[material.metallicRoughness];
		metallicRoughness = metallicRoughnessMap.Sample(anisotropicWrap, input.uv).bg;  // GLTF 2.0 spec.
	}

	if (material.normal > 0)
	{
		// Construct the TBN matrix.
		float3x3 TBN = float3x3(input.tangent, input.bitangent, input.normal);

		Texture2D<float4> normalMap = ResourceDescriptorHeap[material.normal];
		normal = normalMap.Sample(anisotropicWrap, input.uv).rgb;
		normal = normal * 2.0 - 1.0;  // Remap from [0, 1] to [-1, 1].
		normal = normalize(mul(normal, TBN));  // Convert the normal vector from tangent space to world space.
	}

	if (material.occlusion > 0)
	{
		Texture2D<float4> occlusionMap = ResourceDescriptorHeap[material.occlusion];
		ambientOcclusion = occlusionMap.Sample(anisotropicWrap, input.uv).r;
	}

	if (material.emissive > 0)
	{
		Texture2D<float4> emissiveMap = ResourceDescriptorHeap[material.emissive];
		emissive = emissiveMap.Sample(anisotropicWrap, input.uv).rgb;
	}
	
	baseColor *= material.baseColorFactor;
	metallicRoughness *= float2(material.metallicFactor, material.roughnessFactor);
	emissive *= material.emissiveFactor;

	float4 output;
	output.rgb = float3(0.0, 0.0, 0.0);
	output.a = baseColor.a;

	float3 viewDirection = normalize(camera.position.xyz - input.position);
	float3 normalDirection = normal;
	
	Material materialSample;
	materialSample.baseColor = baseColor;
	materialSample.metalness = metallicRoughness.r;
	materialSample.roughness = metallicRoughness.g * metallicRoughness.g;  // Perceptually linear roughness remapping, from observations by Disney.
	materialSample.normal = normal;
	materialSample.occlusion = ambientOcclusion;
	materialSample.emissive = emissive;
	
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[bindData.lightBuffer];
	StructuredBuffer<uint> clusteredLightList = ResourceDescriptorHeap[bindData.clusterData.lightListBuffer];
	StructuredBuffer<uint2> clusteredLightInfo = ResourceDescriptorHeap[bindData.clusterData.lightInfoBuffer];
	StructuredBuffer<float3> sunTransmittance = ResourceDescriptorHeap[bindData.sunTransmittanceBuffer];
	
	uint3 clusterId = DrawToClusterId(FROXEL_SIZE, bindData.clusterData.logY, camera, input.positionCS.xy, input.depthVS);
	uint2 lightInfo = clusteredLightInfo[ClusterId2Index(bindData.clusterData.dimensions, clusterId)];
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
		output.rgb += sample.diffuse.rgb;
	}
	
	TextureCube<float4> irradianceMap = ResourceDescriptorHeap[bindData.iblData.irradianceTexture];
	TextureCube<float4> prefilterMap = ResourceDescriptorHeap[bindData.iblData.prefilterTexture];
	Texture2D<float4> brdfMap = ResourceDescriptorHeap[bindData.iblData.brdfTexture];
	
	float width, height, prefilterMipCount;
	prefilterMap.GetDimensions(0, width, height, prefilterMipCount);
	
	float3 ibl = ComputeIBL(normalDirection, viewDirection, materialSample, bindData.iblData.prefilterLevels, irradianceMap, prefilterMap, brdfMap, anisotropicWrap);
	output.rgb += ibl;
	
	output.rgb += materialSample.emissive;
	
	return output;
}