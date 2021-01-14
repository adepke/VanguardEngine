// Copyright (c) 2019-2021 Andrew Depke

#define RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
	"CBV(b0, visibility = SHADER_VISIBILITY_VERTEX)," \
	"SRV(t0, visibility = SHADER_VISIBILITY_VERTEX)," \
	"CBV(b1, visibility = SHADER_VISIBILITY_VERTEX)"