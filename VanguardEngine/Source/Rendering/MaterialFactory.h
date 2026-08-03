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
	size_t capacity = 0;
	size_t pending = 0;  // Materials in the load queue.

public:
	MaterialFactory(RenderDevice* device, size_t maxMaterials);

	size_t Create();
	void MarkLoaded();
	size_t GetPendingCount() const { return pending; }
};
