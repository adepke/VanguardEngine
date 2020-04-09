// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;

class DescriptorHeap
{
private:
	ResourcePtr<ID3D12DescriptorHeap> Heap;
	size_t CPUHeapStart = 0;
	size_t GPUHeapStart = 0;
	size_t DescriptorSize = 0;  // Increment size.
	size_t AllocatedDescriptors = 0;
	size_t TotalDescriptors = 0;

public:
	auto* Native() const noexcept { return Heap.Get(); }

	void Initialize(RenderDevice& Device, D3D12_DESCRIPTOR_HEAP_TYPE Type, size_t Descriptors);
	D3D12_CPU_DESCRIPTOR_HANDLE Allocate();

	// How do we handle GPU descriptors?

	void SetName(std::wstring_view Name);

	void Reset();
};