// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/DescriptorAllocator.h>
#include <Rendering/Device.h>

void DescriptorAllocator::Initialize(RenderDevice* InDevice, size_t OfflineDescriptors, size_t OnlineDescriptors)
{
	OfflineHeaps[0].Create(DescriptorType::Default, OfflineDescriptors, false);
	OfflineHeaps[1].Create(DescriptorType::Sampler, OfflineDescriptors, false);

	OnlineHeaps[0].resize(InDevice->FrameCount);
	OnlineHeaps[1].resize(InDevice->FrameCount);

	for (auto& Heap : OnlineHeap[0])
	{
		Heap.Create(InDevice, DescriptorType::Default, OnlineDescriptors, true);
	}

	for (auto& Heap : OnlineHeap[1])
	{
		Heap.Create(InDevice, DescriptorType::Sampler, OnlineDescriptors, true);
	}
}

DescriptorHandle DescriptorAllocator::Allocate(DescriptorType Type)
{
	switch (Type)
	{
	case DescriptorType::Default:
	case DescriptorType::Sampler:
		return OfflineHeap[Type].Allocate();
	case DescriptorType::RenderTarget:
		return RenderTargetHeap.Allocate();
	case DescriptorType::DepthStencil:
		return DepthStencilHeap.Allocate();
	}

	return {};
}

void DescriptorAllocator::FrameStep()
{

}