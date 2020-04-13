// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/DescriptorHeap.h>

#include <Core/Windows/DirectX12Minimal.h>

#include <array>
#include <vector>

class RenderDevice;

class DescriptorAllocator
{
private:
	RenderDevice* Device;

	std::array<FreeQueueDescriptorHeap, 2> OfflineHeaps;  // Offline heaps for default and sampler descriptors.
	std::array<std::vector<LinearDescriptorHeap>, 2> OnlineHeaps;  // Online ring buffer heap for default and sampler descriptors.

	FreeQueueDescriptorHeap RenderTargetHeap;
	FreeQueueDescriptorHeap DepthStencilHeap;

public:
	void Initialize(RenderDevice* InDevice, size_t OfflineDescriptors, size_t OnlineDescriptors);

	DescriptorHandle Allocate(DescriptorType Type);

	//void BuildTable(...);
	
	void FrameStep();
};