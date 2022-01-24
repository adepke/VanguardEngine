// Copyright (c) 2019-2022 Andrew Depke

#include "Base.hlsli"
#include "Constants.hlsli"
#include "ImportanceSampling.hlsli"
#include "BRDF.hlsli"
#include "CubeMap.hlsli"

#define RS \
	"RootFlags(0)," \
	"CBV(b0)," \
	"DescriptorTable(" \
		"SRV(t0, space = 3, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"DescriptorTable(" \
		"UAV(u0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"DescriptorTable(" \
		"UAV(u0, space = 2, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE))," \
	"StaticSampler(" \
		"s0," \
		"filter = FILTER_MIN_MAG_MIP_LINEAR," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP)"

SamplerState lutSampler : register(s0);

struct BindData
{
	uint luminanceTexture;
	uint irradianceTexture;
	uint brdfTexture;
	uint cubeFace;
	// Boundary
	uint prefilterMips[PREFILTER_LEVELS];
};

ConstantBuffer<BindData> bindData : register(b0);

// Reference: https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void IrradianceMain(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture2DArray<float4> irradianceMap = textureArraysRW[bindData.irradianceTexture];
	TextureCube luminanceMap = textureCubes[bindData.luminanceTexture];
	
	float width, height, elements;
	irradianceMap.GetDimensions(width, height, elements);
	
	float2 uv = (dispatchId.xy + 0.5f) / width;
	uv = uv * 2.f - 1.f;
	
	float3 forward = normalize(ComputeDirection(uv, dispatchId.z));
	float3 up = float3(0.f, 0.f, 1.f);
	float3 left = normalize(cross(up, forward));
	up = normalize(cross(forward, left));
	
	// Intervals for numerical integration.
	static const int steps = 20;
	float3 irradianceSum = 0.f;
	
	// Traverse a hemisphere in spherical coordinates.
	for (int i = 0; i < steps; ++i)
	{
		float theta = 2.f * pi * ((float)i / float(steps - 1.f));
		for (int j = 0; j < steps; ++j)
		{
			float phi = 0.5f * pi * ((float)j / float(steps - 1.f));
			float3 tangentSpace = normalize(float3(sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi)));
			float3 worldSpace = tangentSpace.x * left + tangentSpace.y * up + tangentSpace.z * forward;
			float3 value = luminanceMap.SampleLevel(lutSampler, worldSpace, 0.f).rgb;
			irradianceSum += value * cos(phi) * sin(phi);  // Scaling improves light distribution.
		}
	}
	
	const float samples = steps * steps;
	irradianceMap[dispatchId] = float4(pi * irradianceSum / samples, 0.f);
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void PrefilterMain(uint3 dispatchId : SV_DispatchThreadID)
{
	// Uses split sum approximation by Epic Games, see: https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
	
	RWTexture2DArray<float4> baseMip = textureArraysRW[bindData.prefilterMips[0]];
	float width, height, depth;
	baseMip.GetDimensions(width, height, depth);
	const float baseMipSize = width;
	const float saTexel = (4.f * pi) / (6.f * baseMipSize * baseMipSize);
	
	const uint maskShift = log2(width);
	const uint xyIndex = dispatchId.y * width + dispatchId.x;
	
	TextureCube luminanceMap = textureCubes[bindData.luminanceTexture];
	
	for (int i = 0; i < PREFILTER_LEVELS; ++i)
	{
		RWTexture2DArray<float4> prefilterMap = textureArraysRW[bindData.prefilterMips[i]];
		prefilterMap.GetDimensions(width, height, depth);
		
		uint groupMask = (1u << i) - 1;
		groupMask |= groupMask << maskShift;
		if ((xyIndex & groupMask) == 0)
		{
			const float roughness = (float)i / (PREFILTER_LEVELS - 1.f);
		
			uint2 pixel = dispatchId.xy / pow(2, i);
			float2 uv = (pixel + 0.5f) / width;
			uv = uv * 2.f - 1.f;
	
			// Assuming normal = view results in reduced highlights at grazing angles.
			float3 normal = normalize(ComputeDirection(uv, bindData.cubeFace));
			float3 reflection = normal;
			float3 view = reflection;
		
			// Sample count.
			const int steps = roughness * 256 + 1;
			float sumWeight = 0.f;
			float3 sumSamples = 0.f;
	
			for (int j = 0; j < steps; ++j)
			{
				float2 sequence = HammersleySet(j, steps);
				float3 halfway = ImportanceSampledGGX(sequence, normal, roughness);
				float3 light = normalize(2.f * dot(view, halfway) * halfway - view);
				float normalDotLight = saturate(dot(normal, light));
				if (normalDotLight > 0.f)
				{
					// Reduce aliasing artifacts from the predictible sequence. Can also jitter the sequence to improve visual fidelity.
					// See: https://chetanjags.wordpress.com/2015/08/26/image-based-lighting/
			
					float D = TrowbridgeReitzGGX(normal, halfway, roughness);
					float normalDotHalfway = saturate(dot(normal, halfway));
					float halfwayDotView = saturate(dot(halfway, view));
					float pdf = D * normalDotHalfway / (4.f * halfwayDotView) + 0.0001f;
					float saSample = 1.f / ((float)steps * pdf + 0.0001f);
					float mip = roughness == 0.f ? 0.f : 0.5f * log2(saSample / saTexel);
					
					sumWeight += normalDotLight;
					sumSamples += luminanceMap.SampleLevel(lutSampler, light, mip).rgb * normalDotLight;
				}
			}
	
			prefilterMap[uint3(pixel, bindData.cubeFace)] = float4(sumSamples / sumWeight, 0.f);
		}
	}
}

[RootSignature(RS)]
[numthreads(8, 8, 1)]
void BRDFMain(uint3 dispatchId : SV_DispatchThreadID)
{
	RWTexture2D<float4> brdfLut = texturesRW[bindData.brdfTexture];
	float width, height;
	brdfLut.GetDimensions(width, height);
	
	float2 uv = (dispatchId.xy + 0.5f) / float2(width, height);
	float normalDotView = uv.x;
	float roughness = 1.f - uv.y;
	
	float3 view = float3(sqrt(1.f - normalDotView * normalDotView), 0.f, normalDotView);
	float3 normal = float3(0.f, 0.f, 1.f);
	
	static const int steps = 1024;
	float a = 0.f;
	float b = 0.f;
	
	for (int i = 0; i < steps; ++i)
	{
		float2 sequence = HammersleySet(i, steps);
		float3 halfway = ImportanceSampledGGX(sequence, normal, roughness);
		float3 light = normalize(2.f * dot(view, halfway) * halfway - view);
		float normalDotLight = saturate(light.z);
		float normalDotHalfway = saturate(halfway.z);
		float viewDotHalfway = saturate(dot(view, halfway));
		if (normalDotLight > 0.f)
		{
			float g = SmithGeometry(normal, view, light, ComputeGeometryConstant(LightingType::IBL, roughness));
			float gVis = (g * viewDotHalfway) / (normalDotHalfway * normalDotView);
			float fresnel = FresnelSchlick(viewDotHalfway, 0.f).x;
			
			a += (1.f - fresnel) * gVis;
			b += fresnel * gVis;
		}
	}
	
	brdfLut[dispatchId.xy] = float4(a / (float)steps, b / (float)steps, 0.f, 0.f);
}