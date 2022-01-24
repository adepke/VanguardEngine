// Copyright (c) 2019-2022 Andrew Depke

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"CBV(b0, visibility = SHADER_VISIBILITY_VERTEX)," \
	"CBV(b1, visibility = SHADER_VISIBILITY_VERTEX)," \
	"SRV(t0, space = 3, visibility = SHADER_VISIBILITY_VERTEX)," \
	"SRV(t1, space = 3, visibility = SHADER_VISIBILITY_VERTEX)," \
	"CBV(b0, space = 3, visibility = SHADER_VISIBILITY_VERTEX)"