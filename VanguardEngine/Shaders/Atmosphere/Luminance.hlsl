// Copyright (c) 2019-2022 Andrew Depke

#include "RootSignature.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "Camera.hlsli"
#include "CubeMap.hlsli"

struct BindData
{
	AtmosphereData atmosphere;
	// Boundary
	uint transmissionTexture;
	uint scatteringTexture;
	uint irradianceTexture;
	float solarZenithAngle;
	// Boundary
	uint luminanceTexture;
	uint cameraBuffer;
	uint cameraIndex;
};

ConstantBuffer<BindData> bindData : register(b0);

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void Main(uint3 dispatchId : SV_DispatchThreadID)
{
	float3 sunDirection = float3(sin(bindData.solarZenithAngle), 0.f, cos(bindData.solarZenithAngle));
	
	RWTexture2DArray<float4> luminanceMap = ResourceDescriptorHeap[bindData.luminanceTexture];
	float width, height, depth;
	luminanceMap.GetDimensions(width, height, depth);
	
	float2 uv = (dispatchId.xy + 0.5f) / width;
	uv = uv * 2.f - 1.f;
	float3 direction = normalize(ComputeDirection(uv, dispatchId.z));
	
	Texture2D<float4> transmittanceLut = ResourceDescriptorHeap[bindData.transmissionTexture];
	Texture3D<float4> scatteringLut = ResourceDescriptorHeap[bindData.scatteringTexture];
	Texture2D<float4> irradianceLut = ResourceDescriptorHeap[bindData.irradianceTexture];

	StructuredBuffer<Camera> cameraBuffer = ResourceDescriptorHeap[bindData.cameraBuffer];
	Camera camera = cameraBuffer[bindData.cameraIndex];
	
	float3 sample = SampleAtmosphere(bindData.atmosphere, camera, direction, sunDirection, false, transmittanceLut, scatteringLut, irradianceLut, bilinearClamp);
	luminanceMap[dispatchId] = float4(sample, 0.f);
}