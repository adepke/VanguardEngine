// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Resource.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/PipelineState.h>

#include <D3D12MemAlloc.h>

#include <memory>
#include <string_view>
#include <array>
#include <utility>
#include <vector>

#include <d3d12.h>
#include <dxgi1_6.h>

class RenderDevice;
struct ResourceDescription;

enum class SyncType
{
	Copy,
	Direct,
	Compute,
};

class RenderDevice
{
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
	ResourcePtr<ID3D12CommandAllocator> CopyCommandAllocator[FrameCount];
	ResourcePtr<ID3D12GraphicsCommandList4> CopyCommandList[FrameCount];

	ResourcePtr<ID3D12CommandQueue> DirectCommandQueue;
	ResourcePtr<ID3D12CommandAllocator> DirectCommandAllocator[FrameCount];  // #TODO: One per worker thread.
	ResourcePtr<ID3D12GraphicsCommandList4> DirectCommandList[FrameCount];  // #TODO: One per worker thread.

	ResourcePtr<ID3D12CommandQueue> ComputeCommandQueue;
	ResourcePtr<ID3D12CommandAllocator> ComputeCommandAllocator[FrameCount];  // #TODO: One per worker thread.
	ResourcePtr<ID3D12GraphicsCommandList4> ComputeCommandList[FrameCount];  // #TODO: One per worker thread.

	ResourcePtr<IDXGISwapChain3> SwapChain;
	size_t Frame = 0;  // Stores the actual frame number. Refers to the current CPU frame being run, stepped after finishing CPU pass.

	std::array<std::shared_ptr<GPUTexture>, FrameCount> BackBufferTextures;  // Render targets bound to the swap chain.

	ResourcePtr<ID3D12Fence> CopyFence;
	HANDLE CopyFenceEvent;
	ResourcePtr<ID3D12Fence> DirectFence;
	HANDLE DirectFenceEvent;
	ResourcePtr<ID3D12Fence> ComputeFence;
	HANDLE ComputeFenceEvent;

	ResourcePtr<D3D12MA::Allocator> Allocator;
	ResourceManager AllocatorManager;

	std::array<std::shared_ptr<GPUBuffer>, FrameCount> FrameBuffers;  // Per-frame shared dynamic heap.
	std::array<size_t, FrameCount> FrameBufferOffsets = {};

	static constexpr size_t ResourceDescriptors = 64;
	static constexpr size_t SamplerDescriptors = 64;
	static constexpr size_t RenderTargetDescriptors = FrameCount * 8;
	static constexpr size_t DepthStencilDescriptors = FrameCount * 8;

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

	// Builds pipelines.
	void ReloadShaders();

	// Resets command lists and allocators.
	void ResetFrame(size_t FrameID);

public:
	RenderDevice(HWND InWindow, bool Software, bool EnableDebugging);
	~RenderDevice();

	auto* Get() const noexcept { return Device.Get(); }

	// Logs various data about the device's feature support. Not needed in optimized builds.
	void CheckFeatureSupport();

	std::shared_ptr<GPUBuffer> Allocate(const ResourceDescription& Description, const std::wstring_view Name);
	void Write(std::shared_ptr<GPUBuffer>& Buffer, const std::vector<uint8_t>& Source, size_t BufferOffset = 0);

	// Allocate a block of CPU write-only, GPU read-only memory from the per-frame dynamic heap.
	std::pair<std::shared_ptr<GPUBuffer>, size_t> FrameAllocate(size_t Size);

	// Sync the specified GPU engine to FrameID. Blocking.
	void Sync(SyncType Type, size_t FrameID);

	// Blocking, waits for the gpu to finish the next frame before returning. Marks the current frame as finished submitting and can move on to the next frame.
	void FrameStep();

	void SetResolution(size_t Width, size_t Height, bool InFullscreen);
};