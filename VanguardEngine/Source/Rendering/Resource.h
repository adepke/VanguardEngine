// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <D3D12MemAlloc.h>

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
	ResourceFrequency updateRate;
	uint32_t bindFlags = 0;  // Determines the view type(s) created.
	uint32_t accessFlags = 0;
};

class ResourceManager;

struct Resource
{
	friend class ResourceManager;

protected:
	ResourcePtr<D3D12MA::Allocation> allocation;

public:
	D3D12_RESOURCE_STATES state;

	Resource() = default;  // #TODO: Prevent creation outside of the resource manager.
	Resource(const Resource&) = delete;
	Resource(Resource&&) noexcept = default;

	Resource& operator=(const Resource&) = delete;
	Resource& operator=(Resource&&) noexcept = default;

	auto* Native() const noexcept { return allocation->GetResource(); }
};