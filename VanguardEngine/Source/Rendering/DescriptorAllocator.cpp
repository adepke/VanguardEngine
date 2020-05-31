// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/DescriptorAllocator.h>
#include <Rendering/Device.h>

// #TEMP
#include <Rendering/Renderer.h>

void DescriptorAllocator::Initialize(RenderDevice* InDevice, size_t OfflineDescriptors, size_t OnlineDescriptors)
{
	OfflineHeaps[0].Create(InDevice, DescriptorType::Default, OfflineDescriptors, false);
	OfflineHeaps[1].Create(InDevice, DescriptorType::Sampler, OfflineDescriptors, false);

	OnlineHeaps[0].resize(InDevice->FrameCount);
	OnlineHeaps[1].resize(InDevice->FrameCount);

	for (auto& Heap : OnlineHeaps[0])
	{
		Heap.Create(InDevice, DescriptorType::Default, OnlineDescriptors, true);
	}

	for (auto& Heap : OnlineHeaps[1])
	{
		Heap.Create(InDevice, DescriptorType::Sampler, OnlineDescriptors, true);
	}

	RenderTargetHeap.Create(InDevice, DescriptorType::RenderTarget, OnlineDescriptors, false);
	DepthStencilHeap.Create(InDevice, DescriptorType::DepthStencil, OnlineDescriptors, false);
}

DescriptorHandle DescriptorAllocator::Allocate(DescriptorType Type)
{
	switch (Type)
	{
	case DescriptorType::Default:
	case DescriptorType::Sampler:
		return OfflineHeaps[static_cast<size_t>(Type)].Allocate();
	case DescriptorType::RenderTarget:
		return RenderTargetHeap.Allocate();
	case DescriptorType::DepthStencil:
		return DepthStencilHeap.Allocate();
	}

	return {};
}

void DescriptorAllocator::AddTableEntry(DescriptorHandle& Handle, DescriptorTableEntryType Type)
{
	PendingTableEntries.push_back(PendingDescriptorTableEntry{ Handle, Type });
}

void DescriptorAllocator::BuildTable(RenderDevice& Device, CommandList& List, uint32_t RootParameter)
{
	D3D12_GPU_DESCRIPTOR_HANDLE TableStart;

	bool First = true;
	for (auto& Entry : PendingTableEntries)
	{
		const auto TargetHeapIndex = Entry.Type == DescriptorTableEntryType::Sampler ? 1 : 0;
		auto& TargetHeap = OnlineHeaps[TargetHeapIndex][Device.GetFrameIndex()];

		auto Destination = TargetHeap.Allocate();

		if (First)
		{
			TableStart = Destination;
			First = false;
		}

		Device.Native()->CopyDescriptorsSimple(1, Destination, Entry.Handle, TargetHeapIndex == 0 ? D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV : D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}

	PendingTableEntries.clear();

	List.Native()->SetGraphicsRootDescriptorTable(RootParameter, TableStart);
}

void DescriptorAllocator::FrameStep(size_t FrameIndex)
{
	for (auto& OnlineHeap : OnlineHeaps)
	{
		OnlineHeap[FrameIndex].Reset();
	}
}