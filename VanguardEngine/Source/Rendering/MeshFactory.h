// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/PrimitiveAssembly.h>
#include <Rendering/RenderComponents.h>

#include <vector>
#include <utility>

class RenderDevice;

struct MeshInstance
{
	size_t assembly = 0;  // Index into a list of primitive assemblies.
	size_t material = 0;  // Index into the material table.
	XMFLOAT4X4 transform;  // Mesh-local space.
	XMFLOAT3 boundingSphereCenter{ 0.f, 0.f, 0.f };  // Subset-local space.
	float boundingSphereRadius = 0.f;
};

class MeshFactory
{
private:
	RenderDevice* device = nullptr;

public:
	BufferHandle indexBuffer;
	BufferHandle vertexPositionBuffer;  // Stores vertex positions.
	BufferHandle vertexExtraBuffer;  // Stores all other vertex attributes.

private:
	size_t indexOffset = 0;
	size_t vertexPositionOffset = 0;
	size_t vertexExtrasOffset = 0;

	PrimitiveOffset AllocateMesh(const std::vector<uint8_t>& vertexPositionData, const std::vector<uint8_t>& vertexExtraData,
		const std::vector<uint8_t>& indexData);

public:
	MeshFactory(RenderDevice* inDevice, size_t maxVertices, size_t maxIndices);
	~MeshFactory();

	MeshComponent CreateMeshComponent(const std::vector<PrimitiveAssembly>& assemblies, const std::vector<MeshInstance>& instances);
};
