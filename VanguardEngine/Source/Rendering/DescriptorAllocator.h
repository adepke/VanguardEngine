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
	D3D12_CPU_DESCRIPTOR_HANDLE Handle;
	DescriptorTableEntryType Type;
};

class RenderDevice;

class DescriptorAllocator
{
	friend class CommandList;

private:
	RenderDevice* Device;

	std::array<FreeQueueDescriptorHeap, 2> OfflineHeaps;  // Offline heaps for default and sampler descriptors.
	std::array<std::vector<LinearDescriptorHeap>, 2> OnlineHeaps;  // Online ring buffer heap for default and sampler descriptors.

	FreeQueueDescriptorHeap RenderTargetHeap;
	FreeQueueDescriptorHeap DepthStencilHeap;

	std::vector<PendingDescriptorTableEntry> PendingTableEntries;

public:
	void Initialize(RenderDevice* InDevice, size_t OfflineDescriptors, size_t OnlineDescriptors);

	DescriptorHandle Allocate(DescriptorType Type);

	void AddTableEntry(DescriptorHandle& Handle, DescriptorTableEntryType Type);

	// Constructs the pending descriptor table and copies descriptors to an online heap.
	void BuildTable(CommandList& List, uint32_t RootParameter);
	
	void FrameStep(size_t FrameIndex);
};