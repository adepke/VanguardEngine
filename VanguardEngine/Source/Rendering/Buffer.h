// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/DescriptorHeap.h>

#include <D3D12MemAlloc.h>

#include <optional>

struct BufferDescription : ResourceDescription
{
	size_t Size;  // Element count. Size * Stride = Byte count.
	size_t Stride;
	std::optional<DXGI_FORMAT> Format;
};

struct Buffer : Resource
{
	friend class ResourceManager;

private:
	std::optional<ResourcePtr<D3D12MA::Allocation>> CounterBuffer;

public:
	BufferDescription Description;

	std::optional<DescriptorHandle> CBV;
	std::optional<DescriptorHandle> SRV;
	std::optional<DescriptorHandle> UAV;
};