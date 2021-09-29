// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/PrimitiveAssembly.h>
#include <Rendering/RenderComponents.h>

#include <vector>
#include <utility>

class RenderDevice;

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

	MeshComponent AllocateMesh(const std::vector<uint8_t>& vertexPositionData, const std::vector<uint8_t>& vertexExtraData, const std::vector<uint8_t>& indexData);

public:
	MeshFactory(RenderDevice* inDevice, size_t maxVertices, size_t maxIndices);
	~MeshFactory();

	inline MeshComponent CreateMeshComponent(const std::vector<PrimitiveAssembly>& assemblies, const std::vector<Material>& materials);
};

inline MeshComponent MeshFactory::CreateMeshComponent(const std::vector<PrimitiveAssembly>& assemblies, const std::vector<Material>& materials)
{
	VGScopedCPUStat("Create Mesh Component");

	// #TEMP: Only one assembly. Use subsets for more than one assembly.
	const auto& assembly = assemblies[0];

	const std::string positionName = "POSITION";
	VGAssert(assembly.vertexStream.contains(positionName), "Primitive assemblies must contain vertex position data.");

	const auto& positionStream = assembly.vertexStream.at(positionName);
	const size_t vertexCount = assembly.GetAttributeCount(positionName);
	std::vector<uint8_t> vertexPositionData{};
	vertexPositionData.resize(vertexCount * assembly.GetAttributeSize(positionName));

	std::memcpy(vertexPositionData.data(), assembly.GetAttributeData(positionName), vertexPositionData.size());

	size_t extraSize = 0;
	for (const auto& [name, stream] : assembly.vertexStream)
	{
		if (name != positionName)
		{
			VGAssert(assembly.GetAttributeCount(name) == vertexCount, "Mismatched vertex attribute counts.");
			extraSize += assembly.GetAttributeSize(name);
		}
	}

	std::vector<uint8_t> vertexExtraData{};
	vertexExtraData.resize(vertexCount * extraSize);

	// Interleave attributes.
	for (size_t i = 0; i < vertexCount; ++i)
	{
		for (const auto& [name, stream] : assembly.vertexStream)
		{
			if (name != positionName)
			{
				const size_t attributeSize = assembly.GetAttributeSize(name);
				std::memcpy(vertexExtraData.data() + i * extraSize, assembly.GetAttributeData(name) + i * attributeSize, attributeSize);
			}
		}
	}

	std::vector<uint8_t> indexData{};
	indexData.resize(assembly.indexStream.size_bytes());
	std::memcpy(indexData.data(), assembly.indexStream.data(), indexData.size());

	return std::move(AllocateMesh(vertexPositionData, vertexExtraData, indexData));
}