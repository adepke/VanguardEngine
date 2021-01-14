// Copyright (c) 2019-2021 Andrew Depke

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"CBV(b0, visibility = SHADER_VISIBILITY_VERTEX)," \
	"SRV(t0, visibility = SHADER_VISIBILITY_VERTEX)," \
	"CBV(b1, visibility = SHADER_VISIBILITY_VERTEX)," \
	"DescriptorTable(" \
		"SRV(t1)," \
		"visibility = SHADER_VISIBILITY_PIXEL)," \
	"StaticSampler(" \
		"s0," \
		"filter = FILTER_MIN_MAG_MIP_LINEAR," \
		"addressU = TEXTURE_ADDRESS_WRAP," \
		"addressV = TEXTURE_ADDRESS_WRAP," \
		"addressW = TEXTURE_ADDRESS_WRAP," \
		"mipLODBias = 0.f," \
		"maxAnisotropy = 0," \
		"comparisonFunc = COMPARISON_ALWAYS," \
		"borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK," \
		"minLOD = 0.f," \
		"maxLOD = 0.f," \
		"space = 0," \
		"visibility = SHADER_VISIBILITY_PIXEL)"