// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/MeshFactory.h>
#include <Rendering/Device.h>

#include <string>
#include <limits>
#include <cstring>
#include <algorithm>

uint32_t SearchVertexChannel(const std::string& name)
{
	if (name == "POSITION") return vertexChannelPosition;
	if (name == "NORMAL") return vertexChannelNormal;
	if (name == "TEXCOORD_0") return vertexChannelTexcoord;
	if (name == "TANGENT") return vertexChannelTangent;
	if (name == "BITANGENT") return vertexChannelBitangent;
	if (name == "COLOR_0") return vertexChannelColor;

	return std::numeric_limits<uint32_t>::max();
}

PrimitiveOffset MeshFactory::AllocateMesh(const std::vector<uint8_t>& vertexPositionData, const std::vector<uint8_t>& vertexExtraData,
	const std::vector<uint8_t>& indexData)
{
	device->GetResourceManager().Write(vertexPositionBuffer, vertexPositionData, vertexPositionOffset);
	device->GetResourceManager().Write(vertexExtraBuffer, vertexExtraData, vertexExtrasOffset);
	device->GetResourceManager().Write(indexBuffer, indexData, indexOffset);

	device->GetDirectList().TransitionBarrier(vertexPositionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	device->GetDirectList().TransitionBarrier(vertexExtraBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	device->GetDirectList().TransitionBarrier(indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	device->GetDirectList().FlushBarriers();

	auto result = PrimitiveOffset{
		.index = indexOffset,
		.position = vertexPositionOffset,
		.extra = vertexExtrasOffset
	};

	vertexPositionOffset += vertexPositionData.size();
	vertexExtrasOffset += vertexExtraData.size();
	indexOffset += indexData.size();

	return result;
}

MeshFactory::MeshFactory(RenderDevice* inDevice, size_t maxVertices, size_t maxIndices)
{
	VGScopedCPUStat("Create Mesh Factory");

	device = inDevice;

	BufferDescription vertexDescription{};
	vertexDescription.size = maxVertices;
	vertexDescription.stride = sizeof(float);  // Indexed by 32 bit chunks (floats, usually).
	vertexDescription.updateRate = ResourceFrequency::Static;
	vertexDescription.bindFlags = BindFlag::ShaderResource;
	vertexDescription.accessFlags = AccessFlag::CPUWrite;
	vertexPositionBuffer = device->GetResourceManager().Create(vertexDescription, VGText("Vertex position buffer"));

	vertexDescription.size *= 8;  // One attribute per element, so increase the size a bit.
	vertexExtraBuffer = device->GetResourceManager().Create(vertexDescription, VGText("Vertex extra attributes buffer"));

	BufferDescription indexDescription{};
	indexDescription.size = maxIndices;
	indexDescription.stride = sizeof(uint32_t);
	indexDescription.updateRate = ResourceFrequency::Static;
	indexDescription.bindFlags = BindFlag::IndexBuffer;
	indexDescription.accessFlags = AccessFlag::CPUWrite;
	indexBuffer = device->GetResourceManager().Create(indexDescription, VGText("Index buffer"));
}

MeshFactory::~MeshFactory()
{
	device->GetResourceManager().Destroy(vertexPositionBuffer);
	device->GetResourceManager().Destroy(vertexExtraBuffer);
	device->GetResourceManager().Destroy(indexBuffer);
}

MeshComponent MeshFactory::CreateMeshComponent(const std::vector<PrimitiveAssembly>& assemblies, const std::vector<MeshInstance>& instances)
{
	VGScopedCPUStat("Create Mesh Component");

	MeshComponent component;

	if (assemblies.empty() || instances.empty())
	{
		return component;
	}

	std::vector<uint8_t> vertexPositionData{};
	std::vector<uint8_t> vertexExtraData{};
	std::vector<uint8_t> indexData{};

	// Create the bitmask of active channels from all assemblies not just the first, in case
	// different assemblies have different layouts. If some assemblies have fewer attributes,
	// they'll be zeros.
	uint32_t channelMask = 1 << vertexChannelPosition;
	uint32_t strides[vertexChannels] = { 0 };
	uint32_t offsets[vertexChannels] = { 0 };
	uint32_t attributeSizes[vertexChannels] = { 0 };

	for (const auto& assembly : assemblies)
	{
		for (const auto& [name, stream] : assembly.vertexStream)
		{
			const auto channelIndex = SearchVertexChannel(name);
			if (channelIndex >= vertexChannels)
			{
				// Unknown/unsupported attribute, ignore.
				continue;
			}

			channelMask |= 1 << channelIndex;
			attributeSizes[channelIndex] = (uint32_t)assembly.GetAttributeSize(name);
		}
	}

	// Positions live in their own buffer, so only the other channels contribute to the
	// interleaved extras stride.
	uint32_t extraStride = 0;
	for (uint32_t i = 0; i < vertexChannels; ++i)
	{
		if (i == vertexChannelPosition || !(channelMask & (1 << i)))
			continue;

		offsets[i] = extraStride;
		extraStride += attributeSizes[i];
	}

	for (uint32_t i = 0; i < vertexChannels; ++i)
	{
		if (!(channelMask & (1 << i)))
			continue;

		strides[i] = (i == vertexChannelPosition) ? attributeSizes[i] : extraStride;
	}

	offsets[vertexChannelPosition] = 0;
	strides[vertexChannelPosition] = attributeSizes[vertexChannelPosition];

	component.metadata.activeChannels = channelMask;
	for (int i = 0; i < vertexChannels; ++i)
	{
		component.metadata.channelStrides[i / 4][i % 4] = strides[i];
		component.metadata.channelOffsets[i / 4][i % 4] = offsets[i];
	}

	// Pack each assembly once, recording where it landed so that instances referencing the
	// same assembly can share it.
	struct AssemblyPlacement
	{
		PrimitiveOffset localOffset;
		size_t vertices = 0;
		size_t indices = 0;
		bool valid = false;
	};

	std::vector<AssemblyPlacement> placements(assemblies.size());
	const std::string positionName = "POSITION";

	for (size_t assemblyIndex = 0; assemblyIndex < assemblies.size(); ++assemblyIndex)
	{
		const auto& assembly = assemblies[assemblyIndex];

		if (!assembly.vertexStream.contains(positionName))
		{
			VGLogError(logRendering, "Primitive assembly {} has no vertex position data, skipping.", assemblyIndex);
			continue;
		}

		auto& placement = placements[assemblyIndex];
		placement.localOffset = PrimitiveOffset{
			.index = indexData.size(),
			.position = vertexPositionData.size(),
			.extra = vertexExtraData.size()
		};

		const size_t vertexCount = assembly.GetAttributeCount(positionName);
		const size_t positionSize = assembly.GetAttributeSize(positionName);
		const size_t positionStride = assembly.GetAttributeStride(positionName);

		vertexPositionData.resize(vertexPositionData.size() + vertexCount * positionSize);
		{
			const uint8_t* source = assembly.GetAttributeData(positionName);
			uint8_t* destination = vertexPositionData.data() + placement.localOffset.position;

			if (positionStride == positionSize)
			{
				std::memcpy(destination, source, vertexCount * positionSize);
			}

			else
			{
				// Interleaved source, so gather element by element.
				for (size_t i = 0; i < vertexCount; ++i)
				{
					std::memcpy(destination + i * positionSize, source + i * positionStride, positionSize);
				}
			}
		}

		vertexExtraData.resize(vertexExtraData.size() + vertexCount * extraStride);

		// Interleave the extra attributes at fixed offsets. Can't write at the channel offsets
		// otherwise assemblies with fewer attribute channels would corrupt.
		for (const auto& [name, stream] : assembly.vertexStream)
		{
			const auto channelIndex = SearchVertexChannel(name);
			if (channelIndex >= vertexChannels || channelIndex == vertexChannelPosition)
				continue;

			const size_t attributeSize = assembly.GetAttributeSize(name);
			const size_t sourceStride = assembly.GetAttributeStride(name);
			VGAssert(assembly.GetAttributeCount(name) == vertexCount, "Mismatched vertex attribute counts.");
			VGAssert(attributeSize == attributeSizes[channelIndex], "Mismatched vertex attribute sizes.");

			const uint8_t* source = assembly.GetAttributeData(name);
			uint8_t* destination = vertexExtraData.data() + placement.localOffset.extra + offsets[channelIndex];

			for (size_t i = 0; i < vertexCount; ++i)
			{
				std::memcpy(destination + i * extraStride, source + i * sourceStride, attributeSize);
			}
		}

		indexData.resize(indexData.size() + assembly.indexStream.size_bytes());
		std::memcpy(indexData.data() + placement.localOffset.index,
			assembly.indexStream.data(), assembly.indexStream.size_bytes());

		placement.vertices = vertexCount;
		placement.indices = assembly.indexStream.size();
		placement.valid = true;
	}

	component.subsets.reserve(instances.size());

	for (const auto& instance : instances)
	{
		if (instance.assembly >= placements.size() || !placements[instance.assembly].valid)
			continue;

		const auto& placement = placements[instance.assembly];

		component.subsets.emplace_back(MeshComponent::Subset{
			.localOffset = placement.localOffset,
			.vertices = placement.vertices,
			.indices = placement.indices,
			.materialIndex = instance.material,
			.transform = instance.transform,
			.boundingSphereCenter = instance.boundingSphereCenter,
			.boundingSphereRadius = instance.boundingSphereRadius
		});
	}

	component.globalOffset = AllocateMesh(vertexPositionData, vertexExtraData, indexData);

	return component;
}
