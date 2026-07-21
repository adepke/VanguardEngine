// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Device.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderGraphResource.h>

#include <entt/entt.hpp>

#include <array>
#include <optional>
#include <unordered_map>

class RenderGraph;
class MeshFactory;

struct AccelerationStructureResources
{
	RenderResource tlasTag;
};

// BLAS built once per unique mesh and cached, TLAS built every frame.
class AccelerationStructures
{
private:
	RenderDevice* device = nullptr;

	// Key by the global vertex buffer position for the mesh. This is a bit hacky and needs
	// to be changed when the global vertex buffer isn't static.
	// #TODO: consider either storing the AS data in the MeshComponent, or a dedicated component?
	std::unordered_map<size_t, BufferHandle> blasCache;

	BufferHandle tlas;
	size_t tlasSize = 0;  // Bytes.

	// Build scratch is grow-only.
	BufferHandle scratch;
	size_t scratchSize = 0;  // Bytes.

	// Per-frame upload buffers holding the TLAS instance descriptions, grow-only.
	std::array<BufferHandle, RenderDevice::frameCount> instanceBuffers = {};
	size_t instanceCapacity = 0;  // Instance descriptor count.

	void EnsureScratch(size_t bytes);
	void EnsureInstanceBuffer(size_t count);
	void EnsureTlas(size_t bytes);

public:
	~AccelerationStructures();
	void Initialize(RenderDevice* inDevice);

	// Builds any missing BLASes and rebuilds the TLAS.
	AccelerationStructureResources Render(RenderGraph& graph, entt::registry& registry, MeshFactory& meshFactory, RenderResource meshPositionTag);
};
