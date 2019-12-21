// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <D3D12MemAlloc.h>  // #NOTE: Can't forward declare D3D12MA::Allocation because then the SFINAE fails in ResourcePtr.

enum class ResourceFrequency
{
	Static,  // Resource is updated at most every few frames.
	Dynamic,  // Resource is updated at least once per frame.
};

enum BindFlag
{
	BindVertexBuffer = 1 << 0,
	BindIndexBuffer = 1 << 1,
	BindConstantBuffer = 1 << 2,
	BindRenderTarget = 1 << 3,
	BindDepthStencil = 1 << 4,
	BindShaderResource = 1 << 5,
	BindUnorderedAccess = 1 << 6,
};

enum AccessFlag
{
	AccessCPURead = 1 << 0,
	AccessCPUWrite = 1 << 1,
	AccessGPUWrite = 1 << 2,  // #TODO: Do we need this?
};

struct ResourceDescription
{
	size_t Size;
	ResourceFrequency UpdateRate;
	uint32_t BindFlags = 0;  // Determines the view type created.
	uint32_t AccessFlags = 0;
};

struct GPUBuffer
{
	ResourcePtr<D3D12MA::Allocation> Resource;

	size_t Size;
	// Usage;
	// Format;
	// Bind flags?
	// CPU access flags?
	CPUHandle View = 0;  // RTV/DSV/SRV/UAV/CBV

	GPUBuffer(ResourcePtr<D3D12MA::Allocation>&& InResource, uint32_t InSize) : Resource(std::move(InResource)), Size(InSize) {}
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

	using GPUBuffer::GPUBuffer;
};