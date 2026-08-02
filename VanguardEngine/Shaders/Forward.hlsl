// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"
#include "Material.hlsli"
#include "Light.hlsli"
#include "Clusters/Clusters.hlsli"
#include "IBL/ImageBasedLighting.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/SkyAmbient.hlsli"
#include "Atmosphere/Visibility.hlsli"

struct ClusterData
{
	uint lightListBuffer;
	uint lightInfoBuffer;
	float logY;
	int froxelSize;
	uint3 dimensions;
	float padding;
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
	uint batchId;
	uint objectBuffer;
	uint cameraBuffer;
	uint cameraIndex;
	uint vertexPositionBuffer;
	uint vertexExtraBuffer;
	uint materialBuffer;
	uint lightBuffer;
	uint atmosphereIrradianceBuffer;
	float globalWeatherCoverage;
	uint weatherTexture;
	uint sunShadowTexture;
	ClusterData clusterData;
	IblData iblData;
	uint2 outputResolution;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VertexIn
{
	uint vertexId : SV_VertexID;
	uint instanceId : SV_InstanceID;
};

struct PixelIn
{
	float4 positionCS : SV_POSITION;  // Clip space in VS, screen space in PS.
	float3 position : POSITION;  // World space.
	float3 normal : NORMAL;  // World space.
	float2 uv : UV;
	float4 tangent : TANGENT;  // World space. Bitangent sign in w component.
	float depthVS : DEPTH;  // View space.
	float4 color : COLOR;
	uint instanceId : SV_InstanceID;
};

[RootSignature(RS)]
PixelIn VSMain(VertexIn input)
{
	StructuredBuffer<ObjectData> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	ObjectData object = objectBuffer[bindData.batchId + input.instanceId];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	
	VertexAssemblyData assemblyData;
	assemblyData.positionBuffer = bindData.vertexPositionBuffer;
	assemblyData.extraBuffer = bindData.vertexExtraBuffer;
	assemblyData.metadata = object.vertexMetadata;
	
	float4 position = LoadVertexPosition(assemblyData, input.vertexId);
	float4 normal = float4(LoadVertexNormal(assemblyData, input.vertexId), 0.f);
	float2 uv = LoadVertexTexcoord(assemblyData, input.vertexId);
	float4 tangent = LoadVertexTangent(assemblyData, input.vertexId);
	float4 color = LoadVertexColor(assemblyData, input.vertexId);
	
	// Support negatively scaled geometry. This flips the handedness of the bitangent sign.
	float mirror = determinant((float3x3)object.worldMatrix) < 0.f ? -1.f : 1.f;

	PixelIn output;
	output.positionCS = position;
	output.positionCS = mul(output.positionCS, object.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.depthVS = output.positionCS.z;
	output.positionCS = mul(output.positionCS, camera.projection);
	output.position = mul(position, object.worldMatrix).xyz;
	output.normal = normalize(mul(normal, object.worldMatrix)).xyz;
	output.uv = uv;
	output.tangent = float4(mul(float4(tangent.xyz, 0.f), object.worldMatrix).xyz, tangent.w * mirror);
	output.color = color;
	output.instanceId = input.instanceId;

	return output;
}

[RootSignature(RS)]
float4 PSMain(PixelIn input, bool frontFace : SV_IsFrontFace) : SV_Target
{
	StructuredBuffer<ObjectData> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	ObjectData object = objectBuffer[bindData.batchId + input.instanceId];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	Camera sunCamera = cameraBuffer[2];  // #TODO: Remove this terrible hardcoding.
	StructuredBuffer<MaterialData> materialBuffer = ResourceDescriptorHeap[bindData.materialBuffer];
	MaterialData material = materialBuffer[object.materialIndex];
	
	// Software backface culling for dynamic material support. #TODO: partition these to get hardware
	// back.
	clip((frontFace || (material.flags & materialFlagDoubleSided)) ? 1 : -1);

	float4 baseColor = input.color;
	if (material.baseColor > 0)
	{
		Texture2D<float4> baseColorMap = ResourceDescriptorHeap[material.baseColor];
		baseColor *= baseColorMap.Sample(anisotropicWrap, input.uv);
	}

	baseColor *= material.baseColorFactor;
	clip(MaterialAlphaTest(material, baseColor.a) ? -1 : 1);

	float2 metallicRoughness = { 1.0, 1.0 };
	// Invert normal for back faces so we get proper lighting.
	float3 normal = frontFace ? input.normal : -input.normal;
	float ambientOcclusion = 1.0;
	float3 emissive = { 1.0, 1.0, 1.0 };

	if (material.metallicRoughness > 0)
	{
		Texture2D<float4> metallicRoughnessMap = ResourceDescriptorHeap[material.metallicRoughness];
		metallicRoughness = metallicRoughnessMap.Sample(anisotropicWrap, input.uv).bg;  // GLTF 2.0 spec.
	}

	if (material.normal > 0)
	{
		// Re-orthonormalize the tangent frame. This isn't free, but the mesh data I work with
		// doesn't provide perfect tangents. Could experiment with asset pre-processing offline
		// to ensure the tangents are high quality here, but for now it's probably not expensive
		// enough to do that.
		float3 t = input.tangent.xyz - normal * dot(normal, input.tangent.xyz);
		float tangentLengthSq = dot(t, t);

		if (tangentLengthSq < 1e-12f)
		{
			// Tangent collapsed onto the normal, it's useless. Come up with some abitrary tangent instead.
			float3 up = abs(normal.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
			t = normalize(cross(up, normal));
		}

		else
		{
			t *= rsqrt(tangentLengthSq);
		}
		
		float3 b = cross(normal, t) * (input.tangent.w < 0.f ? -1.f : 1.f);
		float3x3 TBN = float3x3(t, b, normal);

		Texture2D<float4> normalMap = ResourceDescriptorHeap[material.normal];
		float3 tangentNormal = normalMap.Sample(anisotropicWrap, input.uv).rgb;
		tangentNormal = tangentNormal * 2.0 - 1.0;  // Remap from [0, 1] to [-1, 1].
		normal = normalize(mul(tangentNormal, TBN));  // Convert the normal vector from tangent space to world space.
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
	StructuredBuffer<float3> atmosphereIrradiance = ResourceDescriptorHeap[bindData.atmosphereIrradianceBuffer];
	Texture2D<float3> weatherTexture = ResourceDescriptorHeap[bindData.weatherTexture];
	
	uint3 clusterId = DrawToClusterId(bindData.clusterData.froxelSize, bindData.clusterData.logY, camera, input.positionCS.xy, input.depthVS);
	uint2 lightInfo = clusteredLightInfo[ClusterId2Index(bindData.clusterData.dimensions, clusterId)];
	for (uint i = 0; i < lightInfo.y; ++i)
	{
		uint lightIndex = clusteredLightList[lightInfo.x + i];
		Light light = lights[lightIndex];
		
		// Directional lights are just the combined irradiance of the sun and sky.
		if (light.type == LightType::Directional)
		{
			float3 cameraPositionAtmoSpace = ComputeAtmosphereCameraPosition(camera);
			// Convert to kilometers. The atmosphere should probably provide a helper function to convert, but oh well.
			float3 hitPositionAtmoSpace = input.position / 1000.f;

			const float sunVisibility = CalculateSunVisibility(hitPositionAtmoSpace, light.direction, weatherTexture);
			const float skyVisibility = CalculateSkyVisibility(cameraPositionAtmoSpace, bindData.globalWeatherCoverage);

			// Sun contribution is directional, so feed it into the BRDF path.
			const float3 sunIrradiance = LoadSunIrradianceCamera(atmosphereIrradiance);
			light.color *= sunIrradiance * sunVisibility;
			
			// Screen space shadow mask from the sun.
			if (bindData.sunShadowTexture > 0)
			{
				Texture2D<float2> sunShadowTexture = ResourceDescriptorHeap[bindData.sunShadowTexture];
				light.color *= sunShadowTexture.Load(int3(input.positionCS.xy, 0)).x;
			}
			
			// Sky contribution comes from a SH probe at the camera. Note the clamp is to prevent negatives
			// on sharp peaks.
			const SH::L2_RGB skySH = LoadSkySHCamera(atmosphereIrradiance);
			const float3 skyIrradiance = max(SH::CalculateIrradiance(skySH, normal), 0.0.xxx);
			
			// Feed sky diffuse directly into the output, without going through the directional BRDF, as sky contribution
			// is from the entire hemisphere.
			// #TODO: refactor this out of the light loop.
			const float3 skyDiffuseAlbedo = materialSample.baseColor.rgb * (1.0 - materialSample.metalness);
			output.rgb += (skyVisibility * materialSample.occlusion) * skyIrradiance * skyDiffuseAlbedo / pi;
		}
		
		LightSample sample = SampleLight(light, materialSample, camera, viewDirection, input.position, normalDirection);
		output.rgb += sample.diffuse.rgb;
	}
	
	// Ambient diffuse comes from the SH sky, while ambient specular comes from IBL.
	TextureCube<float4> prefilterMap = ResourceDescriptorHeap[bindData.iblData.prefilterTexture];
	Texture2D<float4> brdfMap = ResourceDescriptorHeap[bindData.iblData.brdfTexture];
	output.rgb += ComputeIBLSpecular(normalDirection, viewDirection, materialSample, bindData.iblData.prefilterLevels,
		prefilterMap, brdfMap, anisotropicWrap);
	
	output.rgb += materialSample.emissive;

	return output;
}