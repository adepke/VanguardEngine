// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Resource.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/PipelineState.h>
#include <Rendering/DescriptorHeap.h>
#include <Rendering/CommandList.h>

#include <D3D12MemAlloc.h>

#include <memory>
#include <string_view>
#include <array>
#include <utility>
#include <vector>
#include <limits>

#include <Core/Windows/DirectX12Minimal.h>
#include <dxgi1_6.h>

// #TEMP
#undef min
#undef max

class RenderDevice;
struct Buffer;
struct Texture;
struct BufferDescription;
struct TextureDescription;

enum class SyncType
{
	Copy,
	Direct,
	Compute,
};

class RenderDevice
{
	friend class ResourceManager;

public:
	bool Debugging = false;
	bool VSync = false;
	size_t RenderWidth = 10;
	size_t RenderHeight = 10;
	bool Fullscreen = false;

	static constexpr uint32_t FrameCount = 3;  // #TODO: Determine at runtime.

private:
	const D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_12_1;

	// #NOTE: Ordering of these variables is significant for proper destruction!
	ResourcePtr<ID3D12Device3> Device;
	ResourcePtr<IDXGIAdapter1> Adapter;

	ResourcePtr<ID3D12CommandQueue> CopyCommandQueue;
	ResourcePtr<CommandList> CopyCommandList[FrameCount];

	ResourcePtr<ID3D12CommandQueue> DirectCommandQueue;
	ResourcePtr<CommandList> DirectCommandList[FrameCount];  // #TODO: One per worker thread.

	ResourcePtr<ID3D12CommandQueue> ComputeCommandQueue;
	ResourcePtr<CommandList> ComputeCommandList[FrameCount];  // #TODO: One per worker thread.

	ResourcePtr<IDXGISwapChain3> SwapChain;
	size_t Frame = 0;  // Stores the actual frame number. Refers to the current CPU frame being run, stepped after finishing CPU pass.

	std::array<std::shared_ptr<Texture>, FrameCount> BackBufferTextures;  // Render targets bound to the swap chain.

	ResourcePtr<ID3D12Fence> CopyFence;
	HANDLE CopyFenceEvent;
	ResourcePtr<ID3D12Fence> DirectFence;
	HANDLE DirectFenceEvent;
	ResourcePtr<ID3D12Fence> ComputeFence;
	HANDLE ComputeFenceEvent;

	ResourcePtr<D3D12MA::Allocator> Allocator;
	ResourceManager AllocatorManager;

	std::array<std::shared_ptr<Buffer>, FrameCount> FrameBuffers;  // Per-frame shared dynamic heap.
	std::array<size_t, FrameCount> FrameBufferOffsets = {};

	static constexpr size_t ResourceDescriptors = 1024;
	static constexpr size_t SamplerDescriptors = 1024;
	static constexpr size_t RenderTargetDescriptors = 1024;
	static constexpr size_t DepthStencilDescriptors = 1024;

	// #NOTE: Shader-visible descriptors require per-frame heaps.
	std::array<DescriptorHeap, FrameCount> ResourceHeaps;  // CBV/SRV/UAV
	std::array<DescriptorHeap, FrameCount> SamplerHeaps;
	DescriptorHeap RenderTargetHeap;
	DescriptorHeap DepthStencilHeap;

	std::vector<PipelineState> PipelineStates;

	ResourcePtr<IDXGIAdapter1> GetAdapter(ResourcePtr<IDXGIFactory7>& Factory, bool Software);

	// Name the D3D objects.
	void SetNames();

	void SetupDescriptorHeaps();
	void SetupRenderTargets();

	// Resets command lists and allocators.
	void ResetFrame(size_t FrameID);

public:
	RenderDevice(HWND InWindow, bool Software, bool EnableDebugging);
	~RenderDevice();

	auto* Native() const noexcept { return Device.Get(); }

	// Logs various data about the device's feature support. Not needed in optimized builds.
	void CheckFeatureSupport();

	// Builds pipelines.
	void ReloadShaders();

	std::shared_ptr<Buffer> CreateResource(const BufferDescription& Description, const std::wstring_view Name);
	std::shared_ptr<Texture> CreateResource(const TextureDescription& Description, const std::wstring_view Name);
	void WriteResource(std::shared_ptr<Buffer>& Target, const std::vector<uint8_t>& Source, size_t TargetOffset = 0);
	void WriteResource(std::shared_ptr<Texture>& Target, const std::vector<uint8_t>& Source, size_t TargetOffset = 0);

	// Allocate a block of CPU write-only, GPU read-only memory from the per-frame dynamic heap.
	std::pair<std::shared_ptr<Buffer>, size_t> FrameAllocate(size_t Size);

	// Sync the specified GPU engine to FrameID. Blocking.
	void Sync(SyncType Type, size_t FrameID = std::numeric_limits<size_t>::max());

	// Blocking, waits for the gpu to finish the next frame before returning. Marks the current frame as finished submitting and can move on to the next frame.
	void FrameStep();
	size_t GetFrameIndex() const noexcept { return Frame % RenderDevice::FrameCount; }

	auto* GetCopyQueue() const noexcept { return CopyCommandQueue.Get(); }
	auto* GetCopyList() const noexcept { return CopyCommandList[GetFrameIndex()].Get(); }
	auto* GetDirectQueue() const noexcept { return DirectCommandQueue.Get(); }
	auto* GetDirectList() const noexcept { return DirectCommandList[GetFrameIndex()].Get(); }
	auto* GetComputeQueue() const noexcept { return ComputeCommandQueue.Get(); }
	auto* GetComputeList() const noexcept { return ComputeCommandList[GetFrameIndex()].Get(); }
	auto* GetSwapChain() const noexcept { return SwapChain.Get(); }
	auto* GetBackBuffer() const noexcept { return BackBufferTextures[GetFrameIndex()].get(); }
	auto& GetResourceHeap() noexcept { return ResourceHeaps[GetFrameIndex()]; }
	auto& GetSamplerHeap() noexcept { return SamplerHeaps[GetFrameIndex()]; }
	auto& GetRenderTargetHeap() noexcept { return RenderTargetHeap; }
	auto& GetDepthStencilHeap() noexcept { return DepthStencilHeap; }

	void SetResolution(size_t Width, size_t Height, bool InFullscreen);
};