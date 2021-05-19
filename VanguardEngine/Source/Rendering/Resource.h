// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/DescriptorHeap.h>

#include <optional>

// #TODO: Fix Windows.h leaking.
#include <D3D12MemAlloc.h>

class ResourceManager;

enum class ResourceFrequency
{
	Static,  // Resource is updated at most every few frames. Default heap.
	Dynamic,  // Resource is updated at least once per frame. Upload heap.
};

enum BindFlag
{
	IndexBuffer = 1 << 0,
	ConstantBuffer = 1 << 1,
	RenderTarget = 1 << 2,
	DepthStencil = 1 << 3,
	ShaderResource = 1 << 4,
	UnorderedAccess = 1 << 5,
};

enum AccessFlag
{
	CPURead = 1 << 0,
	CPUWrite = 1 << 1,
	GPUWrite = 1 << 2,
};

struct BufferDescription
{
	ResourceFrequency updateRate = ResourceFrequency::Dynamic;
	uint32_t bindFlags = 0;  // Determines the view type(s) created.
	uint32_t accessFlags = 0;
	size_t size;  // Element count. Size * Stride = Byte count.
	size_t stride;
	std::optional<DXGI_FORMAT> format;
};

struct TextureDescription
{
	uint32_t bindFlags = 0;  // Determines the view type(s) created.
	uint32_t accessFlags = 0;
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	DXGI_FORMAT format;
	bool mipMapping = false;  // Enables support for multiple mip levels, does not automatically generate mips.
};

struct BufferComponent
{
	ResourcePtr<D3D12MA::Allocation> allocation;
	D3D12_RESOURCE_STATES state;

	BufferDescription description;

	std::optional<DescriptorHandle> CBV;
	std::optional<DescriptorHandle> SRV;
	std::optional<DescriptorHandle> UAV;

	// #TODO: Remove.
	ID3D12Resource* Native() { return allocation->GetResource(); }
};

struct TextureComponent
{
	ResourcePtr<D3D12MA::Allocation> allocation;
	D3D12_RESOURCE_STATES state;

	TextureDescription description;

	std::optional<DescriptorHandle> RTV;
	std::optional<DescriptorHandle> DSV;
	std::optional<DescriptorHandle> SRV;
	// #TODO: UAV support.

	// #TODO: Remove.
	ID3D12Resource* Native() { return allocation->GetResource(); }
};