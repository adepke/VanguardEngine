// Copyright (c) 2019-2022 Andrew Depke

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
	friend class DescriptorHeapBase;
	friend class FreeQueueDescriptorHeap;

private:
	FreeQueueDescriptorHeap* parentHeap = nullptr;  // Optional heap, if we were allocated from a free queue heap.
	uint64_t cpuPointer = 0;
	uint64_t gpuPointer = 0;

public:
	uint32_t bindlessIndex = 0;

public:
	DescriptorHandle() = default;
	DescriptorHandle(const DescriptorHandle&) = delete;
	DescriptorHandle(DescriptorHandle&&) noexcept = default;

	DescriptorHandle& operator=(const DescriptorHandle&) = delete;
	DescriptorHandle& operator=(DescriptorHandle&&) noexcept = default;

	operator D3D12_CPU_DESCRIPTOR_HANDLE() const noexcept { return { cpuPointer }; }
	operator D3D12_GPU_DESCRIPTOR_HANDLE() const noexcept { return { gpuPointer }; }

	void Free();
};

class DescriptorHeapBase
{
protected:
	ResourcePtr<ID3D12DescriptorHeap> heap;
	size_t cpuHeapStart = 0;
	size_t gpuHeapStart = 0;
	size_t descriptorSize = 0;  // Increment size.
	size_t allocatedDescriptors = 0;
	size_t totalDescriptors = 0;

public:
	void Create(RenderDevice* device, DescriptorType type, size_t descriptors, bool visible);

	auto* Native() noexcept { return heap.Get(); }

	size_t GetGPUHeapStart() const { return gpuHeapStart; }
};

class FreeQueueDescriptorHeap : public DescriptorHeapBase
{
private:
	std::queue<DescriptorHandle> freeQueue;

public:
	DescriptorHandle Allocate();
	void Free(DescriptorHandle&& handle);

	~FreeQueueDescriptorHeap();
};

inline void DescriptorHandle::Free()
{
	// parentHeap isn't always valid.
	if (parentHeap)
	{
		parentHeap->Free(std::move(*this));
	}
}