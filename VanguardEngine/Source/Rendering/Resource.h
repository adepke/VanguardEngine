// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <D3D12MemAlloc.h>  // #NOTE: Can't forward declare D3D12MA::Allocation because then the SFINAE fails in ResourcePtr.

enum class ResourceFrequency
{
	Static,  // Resource is updated at most every few frames. Default heap.
	Dynamic,  // Resource is updated at least once per frame. Upload heap.
};

enum BindFlag
{
	VertexBuffer = 1 << 0,
	IndexBuffer = 1 << 1,
	ConstantBuffer = 1 << 2,
	RenderTarget = 1 << 3,
	DepthStencil = 1 << 4,
	ShaderResource = 1 << 5,
	UnorderedAccess = 1 << 6,
};

enum AccessFlag
{
	CPURead = 1 << 0,
	CPUWrite = 1 << 1,
	GPUWrite = 1 << 2,  // #TODO: Do we need this?
};

struct ResourceDescription
{
	size_t Size;
	size_t Stride;
	ResourceFrequency UpdateRate;
	uint32_t BindFlags = 0;  // Determines the view type(s) created.
	uint32_t AccessFlags = 0;
};

struct GPUBuffer
{
	ResourcePtr<D3D12MA::Allocation> Resource;
	ResourceDescription Description;

	CPUHandle CBV = 0;
	CPUHandle SRV = 0;
	CPUHandle UAV = 0;

	GPUBuffer(ResourcePtr<D3D12MA::Allocation>&& InResource, const ResourceDescription& InDesc) : Resource(std::move(InResource)), Description(InDesc) {}
	GPUBuffer(const GPUBuffer&) = delete;
	GPUBuffer(GPUBuffer&&) noexcept = default;

	GPUBuffer& operator=(const GPUBuffer&) = delete;
	GPUBuffer& operator=(GPUBuffer&&) noexcept = default;
};

struct GPUTexture : GPUBuffer
{
	uint32_t Width;
	uint32_t Height;
	uint32_t Depth;

	CPUHandle RTV;
	CPUHandle DSV;

	using GPUBuffer::GPUBuffer;
};