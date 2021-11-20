// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/MeshFactory.h>
#include <Rendering/Device.h>

#include <string>

uint32_t MeshFactory::SearchVertexChannel(const std::string& name)
{
	if (name.find("POSITION") != std::string::npos) return 0;
	if (name.find("NORMAL") != std::string::npos) return 1;
	if (name.find("TEXCOORD") != std::string::npos) return 2;
	if (name.find("TANGENT") != std::string::npos) return 3;
	if (name.find("BITANGENT") != std::string::npos) return 4;
	if (name.find("COLOR") != std::string::npos) return 5;
	return std::numeric_limits<uint32_t>::max();
}

PrimitiveOffset MeshFactory::AllocateMesh(const std::vector<uint8_t>& vertexPositionData, const std::vector<uint8_t>& vertexExtraData, const std::vector<uint8_t>& indexData)
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
	vertexDescription.stride = sizeof(float) * 3;
	vertexDescription.updateRate = ResourceFrequency::Static;
	vertexDescription.bindFlags = BindFlag::ShaderResource;
	vertexDescription.accessFlags = AccessFlag::CPUWrite;
	vertexPositionBuffer = device->GetResourceManager().Create(vertexDescription, VGText("Vertex position buffer"));

	vertexDescription.stride = 1;
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