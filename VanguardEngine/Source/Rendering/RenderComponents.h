// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Device.h>
#include <Rendering/Resource.h>

#include <memory>
#include <cstring>

// #TODO: Array of mesh materials bound to vertex/index offsets to enable multiple materials per mesh.
struct MeshComponent
{
	// #TODO: Material

	std::shared_ptr<GPUBuffer> VertexBuffer;
	std::shared_ptr<GPUBuffer> IndexBuffer;
};

struct CameraComponent
{
	float NearPlane = 0.01f;
	float FarPlane = 10000.f;
	float FieldOfView = 1.57079633f;  // 90 Degrees.

	// #TODO: Position and rotation data?
};

// #TODO: Normals, UVs, etc.
inline MeshComponent CreateMeshComponent(RenderDevice& Device, const std::vector<Vertex>& VertexPositions, const std::vector<uint32_t>& Indices)
{
	VGScopedCPUStat("Create Mesh Component");

	MeshComponent Result;

	ResourceDescription VertexDescription{};
	VertexDescription.Size = VertexPositions.size();
	VertexDescription.Stride = sizeof(Vertex);
	VertexDescription.UpdateRate = ResourceFrequency::Static;
	VertexDescription.BindFlags = BindFlag::VertexBuffer | BindFlag::ShaderResource;
	VertexDescription.AccessFlags = AccessFlag::CPUWrite;

	Result.VertexBuffer = std::move(Device.Allocate(VertexDescription, VGText("Vertex Buffer")));

	std::vector<uint8_t> VertexResource{};
	VertexResource.reserve(sizeof(Vertex) * VertexPositions.size());
	std::memcpy(VertexResource.data(), VertexPositions.data(), VertexResource.size());
	Device.Write(Result.VertexBuffer, VertexResource);

	ResourceDescription IndexDescription{};
	IndexDescription.Size = Indices.size();
	IndexDescription.Stride = sizeof(uint32_t);
	IndexDescription.UpdateRate = ResourceFrequency::Static;
	IndexDescription.BindFlags = BindFlag::IndexBuffer | BindFlag::ShaderResource;
	IndexDescription.AccessFlags = AccessFlag::CPUWrite;

	Result.IndexBuffer = std::move(Device.Allocate(IndexDescription, VGText("Index Buffer")));

	std::vector<uint8_t> IndexResource{};
	IndexResource.reserve(sizeof(uint32_t) * Indices.size());
	std::memcpy(IndexResource.data(), Indices.data(), IndexResource.size());
	Device.Write(Result.IndexBuffer, IndexResource);

	return std::move(Result);
}