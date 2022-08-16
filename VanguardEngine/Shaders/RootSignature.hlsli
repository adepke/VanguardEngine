// Copyright (c) 2019-2022 Andrew Depke

#ifndef __ROOTSIGNATURE_HLSLI__
#define __ROOTSIGNATURE_HLSLI__

#define RS \
	"RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED)," \
	"RootConstants(b0, num32BitConstants = 64)," \
	"StaticSampler(" \
		"s0," \
		"filter = FILTER_MIN_MAG_MIP_POINT," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s1," \
		"filter = FILTER_MIN_MAG_MIP_POINT," \
		"addressU = TEXTURE_ADDRESS_WRAP," \
		"addressV = TEXTURE_ADDRESS_WRAP," \
		"addressW = TEXTURE_ADDRESS_WRAP," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s2," \
		"filter = FILTER_MIN_MAG_MIP_LINEAR," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s3," \
		"filter = FILTER_MIN_MAG_MIP_LINEAR," \
		"addressU = TEXTURE_ADDRESS_WRAP," \
		"addressV = TEXTURE_ADDRESS_WRAP," \
		"addressW = TEXTURE_ADDRESS_WRAP," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s4," \
		"filter = FILTER_ANISOTROPIC," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP," \
		"mipLODBias = 0.f," \
		"maxAnisotropy = 16," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s5," \
		"filter = FILTER_ANISOTROPIC," \
		"addressU = TEXTURE_ADDRESS_WRAP," \
		"addressV = TEXTURE_ADDRESS_WRAP," \
		"addressW = TEXTURE_ADDRESS_WRAP," \
		"mipLODBias = 0.f," \
		"maxAnisotropy = 16," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s6," \
		"filter = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s7," \
		"filter = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
		"addressU = TEXTURE_ADDRESS_WRAP," \
		"addressV = TEXTURE_ADDRESS_WRAP," \
		"addressW = TEXTURE_ADDRESS_WRAP," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s8," \
		"filter = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
		"addressU = TEXTURE_ADDRESS_BORDER," \
		"addressV = TEXTURE_ADDRESS_BORDER," \
		"addressW = TEXTURE_ADDRESS_BORDER," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"borderColor = STATIC_BORDER_COLOR_OPAQUE_BLACK," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s9," \
		"filter = FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT," \
		"addressU = TEXTURE_ADDRESS_CLAMP," \
		"addressV = TEXTURE_ADDRESS_CLAMP," \
		"addressW = TEXTURE_ADDRESS_CLAMP," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_ALL)," \
	"StaticSampler(" \
		"s10," \
		"filter = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
		"addressU = TEXTURE_ADDRESS_BORDER," \
		"addressV = TEXTURE_ADDRESS_BORDER," \
		"addressW = TEXTURE_ADDRESS_BORDER," \
		"mipLODBias = 0.f," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK," \
		"visibility = SHADER_VISIBILITY_ALL)" \

SamplerState pointClamp: register(s0);
SamplerState pointWrap: register(s1);
SamplerState bilinearClamp : register(s2);
SamplerState bilinearWrap : register(s3);
SamplerState anisotropicClamp : register(s4);
SamplerState anisotropicWrap : register(s5);
SamplerState linearMipPointClamp : register(s6);
SamplerState linearMipPointWrap : register(s7);
SamplerState downsampleBorder : register(s8);  // Black border color, see: https://www.froyok.fr/blog/2021-12-ue4-custom-bloom/

SamplerState linearMipPointClampMinimum : register(s9);
SamplerState linearMipPointTransparentBorder : register(s10);  // Border with alpha=0.

#endif  // __ROOTSIGNATURE_HLSLI__