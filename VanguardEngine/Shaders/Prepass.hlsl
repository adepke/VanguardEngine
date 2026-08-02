// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "VertexAssembly.hlsli"
#include "Object.hlsli"
#include "Camera.hlsli"
#include "Material.hlsli"
#include "Utils/OctahedralNormals.hlsli"

struct BindData
{
	uint batchId;
	uint objectBuffer;
	uint cameraBuffer;
	uint cameraIndex;
	uint vertexPositionBuffer;
	uint vertexExtraBuffer;
	uint materialBuffer;
};

ConstantBuffer<BindData> bindData : register(b0);

struct Input
{
	uint vertexId : SV_VertexID;
	uint instanceId : SV_InstanceID;
};

struct Output
{
	float4 positionCS : SV_POSITION;  // Clip space.
	float3 normal : NORMAL;  // World space.
	float2 uv : UV;
	float4 color : COLOR;
	uint instanceId : SV_InstanceID;
};

[RootSignature(RS)]
Output VSMain(Input input)
{
	StructuredBuffer<ObjectData> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	ObjectData object = objectBuffer[bindData.batchId + input.instanceId];
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];

	VertexAssemblyData assemblyData;
	assemblyData.positionBuffer = bindData.vertexPositionBuffer;
	assemblyData.extraBuffer = bindData.vertexExtraBuffer;
	assemblyData.metadata = object.vertexMetadata;

	float4 normal = float4(LoadVertexNormal(assemblyData, input.vertexId), 0.f);

	Output output;
	output.positionCS = LoadVertexPosition(assemblyData, input.vertexId);
	output.positionCS = mul(output.positionCS, object.worldMatrix);
	output.positionCS = mul(output.positionCS, camera.view);
	output.positionCS = mul(output.positionCS, camera.projection);
	output.normal = normalize(mul(normal, object.worldMatrix)).xyz;
	output.uv = LoadVertexTexcoord(assemblyData, input.vertexId);
	output.color = LoadVertexColor(assemblyData, input.vertexId);
	output.instanceId = input.instanceId;

	return output;
}

[RootSignature(RS)]
float2 PSMain(Output input, bool frontFace : SV_IsFrontFace) : SV_Target
{
	StructuredBuffer<ObjectData> objectBuffer = ResourceDescriptorHeap[bindData.objectBuffer];
	ObjectData object = objectBuffer[bindData.batchId + input.instanceId];
	StructuredBuffer<MaterialData> materialBuffer = ResourceDescriptorHeap[bindData.materialBuffer];
	MaterialData material = materialBuffer[object.materialIndex];

	clip((frontFace || (material.flags & materialFlagDoubleSided)) ? 1 : -1);

	float4 baseColor = input.color;

	if (material.baseColor > 0)
	{
		Texture2D<float4> baseColorMap = ResourceDescriptorHeap[material.baseColor];
		baseColor *= baseColorMap.Sample(anisotropicWrap, input.uv);
	}

	baseColor *= material.baseColorFactor;
	clip(MaterialAlphaTest(material, baseColor.a) ? -1 : 1);

	// Invert normal for back faces so we get proper lighting.
	return EncodeNormalOctahedral(normalize(frontFace ? input.normal : -input.normal));
}
