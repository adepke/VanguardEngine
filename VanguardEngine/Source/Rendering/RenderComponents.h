// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Material.h>
#include <Rendering/Device.h>
#include <Rendering/Resource.h>

#include <memory>
#include <vector>
#include <cstring>

struct Material;

// #TODO: Array of mesh materials bound to vertex/index offsets to enable multiple materials per mesh.
struct MeshComponent
{
	struct Subset
	{
		size_t vertexOffset = 0;  // Offset into the vertex buffer.
		size_t indexOffset = 0;  // Offset into the index buffer.
		size_t indices = 0;

		Material material;
	};

	std::vector<Subset> subsets;

	BufferHandle vertexBuffer;
	BufferHandle indexBuffer;
};

struct CameraComponent
{
	float nearPlane = 1.f;
	float farPlane = 5000.f;
	float fieldOfView = 1.57079633f;  // 90 Degrees.
};

struct PointLightComponent
{
	XMFLOAT3 color;
};

inline MeshComponent CreateMeshComponent(RenderDevice& device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	VGScopedCPUStat("Create Mesh Component");

	MeshComponent result;

	BufferDescription vertexDescription{};
	vertexDescription.size = vertices.size();
	vertexDescription.stride = sizeof(Vertex);
	vertexDescription.updateRate = ResourceFrequency::Static;
	vertexDescription.bindFlags = BindFlag::ShaderResource;  // Don't bind as vertex buffer, we aren't using the fixed pipeline vertex processing.
	vertexDescription.accessFlags = AccessFlag::CPUWrite;

	result.vertexBuffer = device.GetResourceManager().Create(vertexDescription, VGText("Vertex buffer"));

	std::vector<uint8_t> vertexResource{};
	vertexResource.resize(sizeof(Vertex) * vertices.size());
	std::memcpy(vertexResource.data(), vertices.data(), vertexResource.size());
	device.GetResourceManager().Write(result.vertexBuffer, vertexResource);

	device.GetDirectList().TransitionBarrier(result.vertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	BufferDescription indexDescription{};
	indexDescription.size = indices.size();
	indexDescription.stride = sizeof(uint32_t);
	indexDescription.updateRate = ResourceFrequency::Static;
	indexDescription.bindFlags = BindFlag::IndexBuffer;
	indexDescription.accessFlags = AccessFlag::CPUWrite;

	result.indexBuffer = device.GetResourceManager().Create(indexDescription, VGText("Index buffer"));

	std::vector<uint8_t> indexResource{};
	indexResource.resize(sizeof(uint32_t) * indices.size());
	std::memcpy(indexResource.data(), indices.data(), indexResource.size());
	device.GetResourceManager().Write(result.indexBuffer, indexResource);

	device.GetDirectList().TransitionBarrier(result.indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	return std::move(result);
}