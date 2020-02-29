// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

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

	std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> CBV;
	std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> SRV;
	std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> UAV;
};