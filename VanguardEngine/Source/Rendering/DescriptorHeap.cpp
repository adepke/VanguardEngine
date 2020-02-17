// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/DescriptorHeap.h>
#include <Rendering/Device.h>

void DescriptorHeap::Initialize(RenderDevice& Device, D3D12_DESCRIPTOR_HEAP_TYPE Type, size_t Descriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
	HeapDesc.Type = Type;
	HeapDesc.NumDescriptors = static_cast<UINT>(Descriptors);
	HeapDesc.Flags = Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HeapDesc.NodeMask = 0;

	auto Result = Device.Native()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(Heap.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create descriptor heap for type '" << Type << "' with " << Descriptors << " descriptors: " << Result;
	}

	CPUHeapStart = Heap->GetCPUDescriptorHandleForHeapStart().ptr;
	GPUHeapStart = Heap->GetGPUDescriptorHandleForHeapStart().ptr;
	DescriptorSize = Device.Native()->GetDescriptorHandleIncrementSize(Type);
	TotalDescriptors = Descriptors;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::Allocate()
{
	VGEnsure(AllocatedDescriptors < TotalDescriptors, "Ran out of descriptor heap memory.");
	AllocatedDescriptors++;

	return { CPUHeapStart + ((AllocatedDescriptors - 1) * DescriptorSize) };
}

void DescriptorHeap::SetName(std::wstring_view Name)
{
	Heap->SetName(Name.data());
}