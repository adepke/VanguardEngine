// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/DescriptorHeap.h>
#include <Rendering/Device.h>

void DescriptorHeapBase::Create(RenderDevice* Device, DescriptorType Type, size_t Descriptors, bool Visible)
{
	D3D12_DESCRIPTOR_HEAP_TYPE HeapType{};

	switch (Type)
	{
	case DescriptorType::Default: HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; break;
	case DescriptorType::Sampler: HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; break;
	case DescriptorType::RenderTarget: HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; break;
	case DescriptorType::DepthStencil: HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; break;
	}

	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
	HeapDesc.Type = HeapType;
	HeapDesc.NumDescriptors = static_cast<UINT>(Descriptors);
	HeapDesc.Flags = Visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HeapDesc.NodeMask = 0;

	auto Result = Device->Native()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(Heap.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create descriptor heap for type '" << Type << "' with " << Descriptors << " descriptors: " << Result;
	}

	CPUHeapStart = Heap->GetCPUDescriptorHandleForHeapStart().ptr;
	GPUHeapStart = Heap->GetGPUDescriptorHandleForHeapStart().ptr;
	DescriptorSize = Device->Native()->GetDescriptorHandleIncrementSize(Type);
	TotalDescriptors = Descriptors;
}

DescriptorHandle LinearDescriptorHeap::Allocate()
{
	VGEnsure(AllocatedDescriptors < TotalDescriptors, "Ran out of linear descriptor heap memory.");
	AllocatedDescriptors++;

	const auto Offset = (AllocatedDescriptors - 1) * DescriptorSize;

	DescriptorHandle Handle{};
	Handle.CPUPointer = CPUHeapStart + Offset;
	Handle.GPUPointer = GPUHeapStart + Offset;

	return Handle;
}

DescriptorHandle FreeQueueDescriptorHeap::Allocate()
{
	// If we have readily available space in the heap, use that first.
	if (AllocatedDescriptors < TotalDescriptors)
	{
		AllocatedDescriptors++;

		const auto Offset = (AllocatedDescriptors - 1) * DescriptorSize;

		DescriptorHandle Handle{};
		Handle.ParentHeap = this;
		Handle.CPUPointer = CPUHeapStart + Offset;
		Handle.GPUPointer = GPUHeapStart + Offset;

		return Handle;
	}

	else
	{
		VGEnsure(!FreeQueue.empty(), "Ran out of free queue descriptor heap memory.");

		Handle = FreeQueue.front();
		FreeQueue.pop();

		return Handle;
	}
}

void FreeQueueDescriptorHeap::Free(DescriptorHandle Handle)
{
	FreeQueue.push(Handle);
}