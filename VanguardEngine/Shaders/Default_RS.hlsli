// Copyright (c) 2019-2021 Andrew Depke

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"RootConstants(b0, num32BitConstants = 4)," \
	"CBV(b1, visibility = SHADER_VISIBILITY_VERTEX)," \
	"CBV(b2, visibility = SHADER_VISIBILITY_ALL)," \
	"CBV(b1, visibility = SHADER_VISIBILITY_PIXEL)," \
	"SRV(t0, space = 1, visibility = SHADER_VISIBILITY_PIXEL)," \
	"SRV(t1, space = 1, visibility = SHADER_VISIBILITY_PIXEL)," \
	"SRV(t2, space = 1, visibility = SHADER_VISIBILITY_PIXEL)," \
	"DescriptorTable(" \
		"SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
		"visibility = SHADER_VISIBILITY_PIXEL)," \
	"SRV(t0, space = 3, visibility = SHADER_VISIBILITY_VERTEX)," \
	"SRV(t1, space = 3, visibility = SHADER_VISIBILITY_VERTEX)," \
	"CBV(b0, space = 3, visibility = SHADER_VISIBILITY_VERTEX)," \
	"StaticSampler(" \
		"s0," \
		"space = 0," \
		"filter = FILTER_ANISOTROPIC," \
		"addressU = TEXTURE_ADDRESS_WRAP," \
		"addressV = TEXTURE_ADDRESS_WRAP," \
		"addressW = TEXTURE_ADDRESS_WRAP," \
		"mipLODBias = 0.f," \
		"maxAnisotropy = 0," \
		"comparisonFunc = COMPARISON_ALWAYS," \
		"borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK," \
		"minLOD = 0.f," \
		"maxLOD = 100.f," \
		"visibility = SHADER_VISIBILITY_PIXEL)"