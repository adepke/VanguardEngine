// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/DescriptorAllocator.h>
#include <Rendering/Device.h>

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

void DescriptorAllocator::FrameStep(size_t FrameIndex)
{
	for (auto& OnlineHeap : OnlineHeaps)
	{
		OnlineHeap[FrameIndex].Reset();
	}
}