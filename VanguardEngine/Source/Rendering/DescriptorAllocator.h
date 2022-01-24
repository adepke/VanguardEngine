// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/DescriptorHeap.h>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;

class DescriptorAllocator
{
	friend class CommandList;

private:
	RenderDevice* device;

	FreeQueueDescriptorHeap defaultHeap;
	FreeQueueDescriptorHeap defaultNonVisibleHeap;
	FreeQueueDescriptorHeap renderTargetHeap;
	FreeQueueDescriptorHeap depthStencilHeap;

public:
	void Initialize(RenderDevice* inDevice, size_t shaderDescriptors, size_t renderTargetDescriptors, size_t depthStencilDescriptors);

	DescriptorHandle Allocate(DescriptorType type);
	DescriptorHandle AllocateNonVisible();  // Used to obtain a default descriptor in a non-visible heap.

	D3D12_GPU_DESCRIPTOR_HANDLE GetBindlessHeap() const;
	
	void FrameStep(size_t frameIndex);
};

inline D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetBindlessHeap() const
{
	return { defaultHeap.GetGPUHeapStart() };
}