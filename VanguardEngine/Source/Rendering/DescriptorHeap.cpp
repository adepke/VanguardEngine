// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/DescriptorHeap.h>
#include <Rendering/Device.h>
#include <Rendering/Resource.h>

#include <limits>

void DescriptorHeapBase::Create(RenderDevice* device, DescriptorType type, size_t descriptors, bool visible)
{
	VGScopedCPUStat("Descriptor Heap Create");

	D3D12_DESCRIPTOR_HEAP_TYPE heapType{};

	switch (type)
	{
	case DescriptorType::Default: heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; break;
	case DescriptorType::Sampler: heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; break;
	case DescriptorType::RenderTarget: heapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; break;
	case DescriptorType::DepthStencil: heapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; break;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = heapType;
	heapDesc.NumDescriptors = static_cast<UINT>(descriptors);
	heapDesc.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.NodeMask = 0;

	auto result = device->Native()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(heap.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create descriptor heap for type '" << static_cast<int>(type) << "' with " << descriptors << " descriptors: " << result;
	}

	cpuHeapStart = heap->GetCPUDescriptorHandleForHeapStart().ptr;
	gpuHeapStart = std::numeric_limits<size_t>::max();  // Non-visible heaps cannot call GetGPUDescriptorHandleForHeapStart().
	descriptorSize = device->Native()->GetDescriptorHandleIncrementSize(heapType);
	totalDescriptors = descriptors;

	if (visible)
	{
		gpuHeapStart = heap->GetGPUDescriptorHandleForHeapStart().ptr;
	}
}

DescriptorHandle FreeQueueDescriptorHeap::Allocate()
{
	VGScopedCPUStat("Descriptor Heap Allocate");

	// If we have readily available space in the heap, use that first.
	if (allocatedDescriptors < totalDescriptors)
	{
		++allocatedDescriptors;

		const auto offset = (allocatedDescriptors - 1) * descriptorSize;

		DescriptorHandle handle{};
		handle.parentHeap = this;
		handle.cpuPointer = cpuHeapStart + offset;
		handle.gpuPointer = gpuHeapStart + offset;
		handle.bindlessIndex = allocatedDescriptors - 1;

		return handle;
	}

	else
	{
		VGEnsure(!freeQueue.empty(), "Ran out of free queue descriptor heap memory.");

		auto handle = std::move(freeQueue.front());
		freeQueue.pop();

		return handle;
	}
}

void FreeQueueDescriptorHeap::Free(DescriptorHandle&& handle)
{
	VGScopedCPUStat("Descriptor Heap Free");

	freeQueue.push(std::move(handle));
}

FreeQueueDescriptorHeap::~FreeQueueDescriptorHeap()
{
	// Discard any remaining descriptors in the free queue.
	while (freeQueue.size())
	{
		freeQueue.front().parentHeap = nullptr;
		freeQueue.pop();
	}
}