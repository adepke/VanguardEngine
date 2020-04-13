// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <Core/Windows/DirectX12Minimal.h>

#include <memory>
#include <queue>

class RenderDevice;

enum class DescriptorType
{
	Default = 0,
	Sampler = 1,
	RenderTarget = 2,
	DepthStencil = 3,
};

struct DescriptorHandle
{
private:
	class FreeQueueDescriptorHeap* ParentHeap = nullptr;  // Optional heap, if we were allocated from a free list heap.
	uint64_t CPUPointer;
	uint64_t GPUPointer;

public:
	~DescriptorHandle() { if (ParentHeap) ParentHeap->Free(*this); }

	explicit operator D3D12_CPU_DESCRIPTOR_HANDLE() noexcept const { return { CPUPointer }; }
	explicit operator D3D12_GPU_DESCRIPTOR_HANDLE() noexcept const { return { GPUPointer }; }
};

class DescriptorHeapBase
{
	ResourcePtr<ID3D12DescriptorHeap> Heap;
	size_t CPUHeapStart = 0;
	size_t GPUHeapStart = 0;
	size_t DescriptorSize = 0;  // Increment size.
	size_t AllocatedDescriptors = 0;
	size_t TotalDescriptors = 0;

	void Create(RenderDevice* Device, DescriptorType Type, size_t Descriptors, bool Visible);
};

class LinearDescriptorHeap : DescriptorHeapBase
{
public:
	DescriptorHandle Allocate();
};

class FreeQueueDescriptorHeap : DescriptorHeapBase
{
private:
	std::queue<DescriptorHandle> FreeQueue;

public:
	DescriptorHandle Allocate();
	void Free(DescriptorHandle Handle);
};