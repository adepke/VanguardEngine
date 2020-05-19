// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <Core/Windows/DirectX12Minimal.h>

#include <memory>
#include <queue>

class RenderDevice;
class FreeQueueDescriptorHeap;

enum class DescriptorType
{
	Default = 0,
	Sampler = 1,
	RenderTarget = 2,
	DepthStencil = 3,
};

struct DescriptorHandle
{
	friend class LinearDescriptorHeap;
	friend class FreeQueueDescriptorHeap;

private:
	FreeQueueDescriptorHeap* ParentHeap = nullptr;  // Optional heap, if we were allocated from a free queue heap.
	uint64_t CPUPointer;
	uint64_t GPUPointer;

public:
	inline ~DescriptorHandle();

	DescriptorHandle() = default;
	DescriptorHandle(const DescriptorHandle&) = delete;
	DescriptorHandle(DescriptorHandle&&) noexcept = default;

	DescriptorHandle& operator=(const DescriptorHandle&) = delete;
	DescriptorHandle& operator=(DescriptorHandle&&) noexcept = default;

	operator D3D12_CPU_DESCRIPTOR_HANDLE() noexcept { return { CPUPointer }; }
	operator D3D12_GPU_DESCRIPTOR_HANDLE() noexcept { return { GPUPointer }; }
};

class DescriptorHeapBase
{
protected:
	ResourcePtr<ID3D12DescriptorHeap> Heap;
	size_t CPUHeapStart = 0;
	size_t GPUHeapStart = 0;
	size_t DescriptorSize = 0;  // Increment size.
	size_t AllocatedDescriptors = 0;
	size_t TotalDescriptors = 0;

public:
	void Create(RenderDevice* Device, DescriptorType Type, size_t Descriptors, bool Visible);

	auto* Native() noexcept { return Heap.Get(); }
};

class LinearDescriptorHeap : public DescriptorHeapBase
{
public:
	DescriptorHandle Allocate();
	void Reset() { AllocatedDescriptors = 0; }
};

class FreeQueueDescriptorHeap : public DescriptorHeapBase
{
private:
	std::queue<DescriptorHandle> FreeQueue;

public:
	DescriptorHandle Allocate();
	void Free(DescriptorHandle&& Handle);

	~FreeQueueDescriptorHeap();
};

inline DescriptorHandle::~DescriptorHandle()
{
	if (ParentHeap)
	{
		ParentHeap->Free(std::move(*this));
	}
}