// Copyright (c) 2019-2022 Andrew Depke

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

	uint32_t SearchVertexChannel(const std::string& name);
	PrimitiveOffset AllocateMesh(const std::vector<uint8_t>& vertexPositionData, const std::vector<uint8_t>& vertexExtraData, const std::vector<uint8_t>& indexData);

public:
	MeshFactory(RenderDevice* inDevice, size_t maxVertices, size_t maxIndices);
	~MeshFactory();

	inline MeshComponent CreateMeshComponent(const std::vector<PrimitiveAssembly>& assemblies, const std::vector<size_t>& materials, const std::vector<uint32_t>& materialIndices);
};

inline MeshComponent MeshFactory::CreateMeshComponent(const std::vector<PrimitiveAssembly>& assemblies, const std::vector<size_t>& materials, const std::vector<uint32_t>& materialIndices)
{
	VGScopedCPUStat("Create Mesh Component");

	MeshComponent component;
	std::vector<uint8_t> vertexPositionData{};
	std::vector<uint8_t> vertexExtraData{};
	std::vector<uint8_t> indexData{};

	// Create the bitmask of active channels and compute the strides/offsets for just the first assembly.
	// This implies the assumption that all mesh subsets within a mesh component have the same vertex layout.
	uint32_t channelMask = 1 << vertexChannelPosition;
	uint32_t strides[vertexChannels] = { 0 };
	uint32_t offsets[vertexChannels] = { 0 };
	size_t offset = 0;
	for (const auto& [name, stream] : assemblies.front().vertexStream)
	{
		const auto channelIndex = SearchVertexChannel(name);
		channelMask |= 1 << channelIndex;
		const auto attributeSize = assemblies.front().GetAttributeSize(name);
		offsets[channelIndex] = offset;
		// The position channel offset doesn't affect the extras buffer.
		if (channelIndex > 0)
		{
			offset += attributeSize;
		}
	}

	for (const auto& [name, stream] : assemblies.front().vertexStream)
	{
		const auto channelIndex = SearchVertexChannel(name);
		if (channelIndex > 0)
		{
			strides[channelIndex] = offset;
		}

		else
		{
			// Position channel has an isolated stride.
			strides[channelIndex] = assemblies.front().GetAttributeSize(name);
		}
	}

	VGAssert(offsets[vertexChannelPosition] == 0, "Incorrect vertex position offset.");
	component.metadata.activeChannels = channelMask;
	for (int i = 0; i < vertexChannels; ++i)
	{
		component.metadata.channelStrides[i / 4][i % 4] = strides[i];
		component.metadata.channelOffsets[i / 4][i % 4] = offsets[i];
	}

	component.subsets.reserve(assemblies.size());

	uint32_t index = 0;
	for (const auto& assembly : assemblies)
	{
		PrimitiveOffset localOffset{
			.index = indexData.size(),
			.position = vertexPositionData.size(),
			.extra = vertexExtraData.size()
		};

		const std::string positionName = "POSITION";
		VGAssert(assembly.vertexStream.contains(positionName), "Primitive assemblies must contain vertex position data.");

		const auto& positionStream = assembly.vertexStream.at(positionName);
		const size_t vertexCount = assembly.GetAttributeCount(positionName);
		
		vertexPositionData.resize(vertexPositionData.size() + vertexCount * assembly.GetAttributeSize(positionName));
		std::memcpy(vertexPositionData.data() + localOffset.position, assembly.GetAttributeData(positionName), vertexPositionData.size() - localOffset.position);

		size_t extraSize = 0;
		for (const auto& [name, stream] : assembly.vertexStream)
		{
			if (name != positionName)
			{
				VGAssert(assembly.GetAttributeCount(name) == vertexCount, "Mismatched vertex attribute counts.");
				extraSize += assembly.GetAttributeSize(name);
			}
		}

		vertexExtraData.resize(vertexExtraData.size() + vertexCount * extraSize);

		// Interleave attributes.
		for (size_t i = 0; i < vertexCount; ++i)
		{
			size_t attributeOffset = 0;
			for (const auto& [name, stream] : assembly.vertexStream)
			{
				if (name != positionName)
				{
					const size_t attributeSize = assembly.GetAttributeSize(name);
					std::memcpy(vertexExtraData.data() + localOffset.extra + i * extraSize + attributeOffset, assembly.GetAttributeData(name) + i * attributeSize, attributeSize);
					attributeOffset += attributeSize;
				}
			}
		}

		indexData.resize(indexData.size() + assembly.indexStream.size_bytes());
		std::memcpy(indexData.data() + localOffset.index, assembly.indexStream.data(), indexData.size() - localOffset.index);

		component.subsets.emplace_back(localOffset, assembly.indexStream.size(), materials[materialIndices[index]]);

		++index;
	}

	component.globalOffset = AllocateMesh(vertexPositionData, vertexExtraData, indexData);

	return component;
}