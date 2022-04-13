// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>

class RenderDevice;

class MaterialFactory
{
public:
	BufferHandle materialBuffer;

private:
	size_t count = 0;

public:
	MaterialFactory(RenderDevice* device, size_t maxMaterials);

	size_t Create();
};