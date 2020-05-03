// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Material.h>
#include <Rendering/Device.h>
#include <Rendering/Buffer.h>

#include <memory>
#include <vector>
#include <cstring>

struct Material;

// #TODO: Array of mesh materials bound to vertex/index offsets to enable multiple materials per mesh.
struct MeshComponent
{
	struct Subset
	{
		size_t VertexOffset = 0;  // Offset into the vertex buffer.
		size_t Vertices = 0;

		std::shared_ptr<Material> Mat;
	};

	std::vector<Subset> Subsets;

	std::shared_ptr<Buffer> VertexBuffer;
	std::shared_ptr<Buffer> IndexBuffer;
};

struct CameraComponent
{
	float NearPlane = 0.01f;
	float FarPlane = 10000.f;
	float FieldOfView = 1.57079633f;  // 90 Degrees.

	// #TODO: Derive from a transform component.
	XMFLOAT3 FocusPosition{ 0.f, 0.f, 0.f };
	XMFLOAT3 UpDirection{ 0.f, 0.f, 1.f };
};

// #TODO: Normals, UVs, etc.
inline MeshComponent CreateMeshComponent(RenderDevice& Device, const std::vector<Vertex>& VertexPositions, const std::vector<uint32_t>& Indices)
{
	VGScopedCPUStat("Create Mesh Component");

	MeshComponent Result;

	BufferDescription VertexDescription{};
	VertexDescription.Size = VertexPositions.size();
	VertexDescription.Stride = sizeof(Vertex);
	VertexDescription.UpdateRate = ResourceFrequency::Static;
	VertexDescription.BindFlags = BindFlag::ShaderResource;  // Don't bind as vertex buffer, we aren't using the fixed pipeline vertex processing.
	VertexDescription.AccessFlags = AccessFlag::CPUWrite;

	Result.VertexBuffer = std::move(Device.CreateResource(VertexDescription, VGText("Vertex Buffer")));

	std::vector<uint8_t> VertexResource{};
	VertexResource.resize(sizeof(Vertex) * VertexPositions.size());
	std::memcpy(VertexResource.data(), VertexPositions.data(), VertexResource.size());
	Device.WriteResource(Result.VertexBuffer, VertexResource);

	Device.GetDirectList().TransitionBarrier(Result.VertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	BufferDescription IndexDescription{};
	IndexDescription.Size = Indices.size();
	IndexDescription.Stride = sizeof(uint32_t);
	IndexDescription.UpdateRate = ResourceFrequency::Static;
	IndexDescription.BindFlags = BindFlag::IndexBuffer;
	IndexDescription.AccessFlags = AccessFlag::CPUWrite;

	Result.IndexBuffer = std::move(Device.CreateResource(IndexDescription, VGText("Index Buffer")));

	std::vector<uint8_t> IndexResource{};
	IndexResource.resize(sizeof(uint32_t) * Indices.size());
	std::memcpy(IndexResource.data(), Indices.data(), IndexResource.size());
	Device.WriteResource(Result.IndexBuffer, IndexResource);

	Device.GetDirectList().TransitionBarrier(Result.IndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	Device.GetDirectList().FlushBarriers();

	return std::move(Result);
}