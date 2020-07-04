// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/DescriptorHeap.h>

#include <Core/Windows/DirectX12Minimal.h>

#include <array>
#include <vector>

enum class DescriptorTableEntryType
{
	ShaderResource,
	UnorderedAccess,
	ConstantBuffer,
	Sampler,
};

struct PendingDescriptorTableEntry
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle;
	DescriptorTableEntryType type;
};

class RenderDevice;

class DescriptorAllocator
{
	friend class CommandList;

private:
	RenderDevice* device;

	std::array<FreeQueueDescriptorHeap, 2> offlineHeaps;  // Offline heaps for default and sampler descriptors.
	std::array<std::vector<LinearDescriptorHeap>, 2> onlineHeaps;  // Online ring buffer heap for default and sampler descriptors.

	FreeQueueDescriptorHeap renderTargetHeap;
	FreeQueueDescriptorHeap depthStencilHeap;

	std::vector<PendingDescriptorTableEntry> pendingTableEntries;

public:
	void Initialize(RenderDevice* inDevice, size_t offlineDescriptors, size_t onlineDescriptors);

	DescriptorHandle Allocate(DescriptorType type);

	void AddTableEntry(DescriptorHandle& handle, DescriptorTableEntryType type);

	// Constructs the pending descriptor table and copies descriptors to an online heap.
	void BuildTable(RenderDevice& device, CommandList& list, uint32_t rootParameter);
	
	void FrameStep(size_t frameIndex);
};