#define RS \
	"RootFlags(" \
		"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
		"DENY_HULL_SHADER_ROOT_ACCESS |" \
		"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
		"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
	"RootConstants(num32BitConstants = 16, b0, visibility = SHADER_VISIBILITY_VERTEX)," \
	"SRV(t0, visibility = SHADER_VISIBILITY_VERTEX)," \
	"DescriptorTable(" \
		"SRV(t1, numDescriptors = 1)," \
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