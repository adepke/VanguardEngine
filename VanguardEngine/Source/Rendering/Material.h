// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

struct Material
{
	BufferHandle materialBuffer;
	bool transparent = false;
};