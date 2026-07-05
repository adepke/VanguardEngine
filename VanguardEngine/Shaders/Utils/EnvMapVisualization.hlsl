// Copyright (c) 2019-2022 Andrew Depke

// This shader draws a ray traced sphere visualizing an environment cubemap (e.g. the IBL
// luminance or prefilter cube).
// Set mipLevel to inspect a specific mip / prefilter roughness bin.
//
// Example render layout:
//
// BlendMode alphaBlend{
// 	  .srcBlend = D3D12_BLEND_SRC_ALPHA,
// 	  .destBlend = D3D12_BLEND_INV_SRC_ALPHA,
// 	  .blendOp = D3D12_BLEND_OP_ADD,
// 	  .srcBlendAlpha = D3D12_BLEND_ONE,
// 	  .destBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
// 	  .blendOpAlpha = D3D12_BLEND_OP_ADD,
// };
// auto envLayout = RenderPipelineLayout{}
// 	  .VertexShader({ "Utils/EnvMapVisualization", "VSMain" })
// 	  .PixelShader({ "Utils/EnvMapVisualization", "PSMain" })
// 	  .BlendMode(true, alphaBlend)
// 	  .DepthEnabled(false);

#include "RootSignature.hlsli"
#include "Camera.hlsli"
#include "Geometry.hlsli"

struct BindData
{
	float3 position;
	float radius;
	uint cameraBuffer;
	uint cameraIndex;
	uint cubeTexture;
	float mipLevel;
};

ConstantBuffer<BindData> bindData : register(b0);

struct VSInput
{
	uint vertexID : SV_VertexID;
};

struct PSInput
{
	float4 positionCS : SV_POSITION;
	float2 uv : UV;
};

[RootSignature(RS)]
PSInput VSMain(VSInput input)
{
	PSInput output;
	output.uv = float2((input.vertexID << 1) & 2, input.vertexID & 2);
	output.positionCS = float4((output.uv.x - 0.5) * 2.0, -(output.uv.y - 0.5) * 2.0, 0, 1);

	return output;
}

[RootSignature(RS)]
float4 PSMain(PSInput input) : SV_Target
{
	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	TextureCube<float4> cubeMap = ResourceDescriptorHeap[bindData.cubeTexture];

	float3 dir = ComputeRayDirection(camera, input.uv);

	float2 solutions;
	if (RaySphereIntersection(camera.position.xyz, dir, bindData.position, bindData.radius, solutions))
	{
		float3 surfacePoint = camera.position.xyz + dir * solutions.x;
		float3 normal = normalize(surfacePoint - bindData.position);

		// Sample the cube along the surface normal. Swap to reflect(dir, normal) for a chrome-ball look.
		float3 radiance = cubeMap.SampleLevel(bilinearClamp, normal, bindData.mipLevel).rgb;

		float exposure = 1.f;  // May not need exposure if this is happening before the post process.
		return float4(radiance * exposure, 1.f);
	}

	return 0.xxxx;
}
