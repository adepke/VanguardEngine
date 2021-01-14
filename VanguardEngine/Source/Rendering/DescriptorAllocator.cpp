// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/DescriptorAllocator.h>
#include <Rendering/Device.h>

// #TEMP
#include <Rendering/Renderer.h>

void DescriptorAllocator::Initialize(RenderDevice* inDevice, size_t offlineDescriptors, size_t onlineDescriptors)
{
	VGScopedCPUStat("Descriptor Allocator Initialize");

	offlineHeaps[0].Create(inDevice, DescriptorType::Default, offlineDescriptors, false);
	offlineHeaps[1].Create(inDevice, DescriptorType::Sampler, offlineDescriptors, false);

	onlineHeaps[0].resize(inDevice->frameCount);
	onlineHeaps[1].resize(inDevice->frameCount);

	for (auto& heap : onlineHeaps[0])
	{
		heap.Create(inDevice, DescriptorType::Default, onlineDescriptors, true);
	}

	for (auto& heap : onlineHeaps[1])
	{
		heap.Create(inDevice, DescriptorType::Sampler, onlineDescriptors, true);
	}

	renderTargetHeap.Create(inDevice, DescriptorType::RenderTarget, onlineDescriptors, false);
	depthStencilHeap.Create(inDevice, DescriptorType::DepthStencil, onlineDescriptors, false);
}

DescriptorHandle DescriptorAllocator::Allocate(DescriptorType type)
{
	switch (type)
	{
	case DescriptorType::Default:
	case DescriptorType::Sampler:
		return offlineHeaps[static_cast<size_t>(type)].Allocate();
	case DescriptorType::RenderTarget:
		return renderTargetHeap.Allocate();
	case DescriptorType::DepthStencil:
		return depthStencilHeap.Allocate();
	}

	return {};
}

void DescriptorAllocator::AddTableEntry(DescriptorHandle& handle, DescriptorTableEntryType type)
{
	pendingTableEntries.push_back(PendingDescriptorTableEntry{ handle, type });
}

void DescriptorAllocator::BuildTable(RenderDevice& device, CommandList& list, uint32_t rootParameter)
{
	VGScopedCPUStat("Build Descriptor Table");

	D3D12_GPU_DESCRIPTOR_HANDLE tableStart{};

	bool first = true;
	for (auto& entry : pendingTableEntries)
	{
		const auto targetHeapIndex = entry.type == DescriptorTableEntryType::Sampler ? 1 : 0;
		auto& targetHeap = onlineHeaps[targetHeapIndex][device.GetFrameIndex()];

		auto destination = targetHeap.Allocate();

		if (first)
		{
			tableStart = destination;
			first = false;
		}

		device.Native()->CopyDescriptorsSimple(1, destination, entry.handle, targetHeapIndex == 0 ? D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV : D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}

	pendingTableEntries.clear();

	list.Native()->SetGraphicsRootDescriptorTable(rootParameter, tableStart);
}

void DescriptorAllocator::FrameStep(size_t frameIndex)
{
	VGScopedCPUStat("Descriptor Allocator Frame Step");

	for (auto& onlineHeap : onlineHeaps)
	{
		onlineHeap[frameIndex].Reset();
	}
}