// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/ResourceHandle.h>

struct Material
{
	BufferHandle materialBuffer;
	bool transparent = false;
};