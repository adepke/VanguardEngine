// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/PipelineState.h>
#include <Rendering/DescriptorAllocator.h>
#include <Rendering/CommandList.h>

// #TODO: Fix Windows.h leaking.
#include <Rendering/ResourceHandle.h>
#include <Rendering/Adapter.h>  // #TODO: Including this before D3D12MemAlloc.h causes an array of errors, this needs to be fixed.

#include <memory>
#include <string_view>
#include <array>
#include <utility>
#include <vector>
#include <limits>

class RenderDevice
{
	friend class ResourceManager;

public:
	bool debugging = false;
	bool vSync = false;
	uint32_t renderWidth = 10;
	uint32_t renderHeight = 10;
	bool supportsRayTracing = false;

	static constexpr uint32_t frameCount = 3;  // #TODO: Determine at runtime.

private:
	const D3D_FEATURE_LEVEL targetFeatureLevel = D3D_FEATURE_LEVEL_12_1;
	const D3D_SHADER_MODEL targetShaderModel = D3D_SHADER_MODEL_6_3;
	uint32_t swapChainFlags = 0;

	// #NOTE: Ordering of these variables is significant for proper destruction!
	ResourcePtr<ID3D12Device5> device;
	Adapter renderAdapter;

	ResourcePtr<ID3D12CommandQueue> directCommandQueue;
	TracyD3D12Ctx directContext;
	CommandList directCommandList[frameCount];  // #TODO: One per worker thread.

	ResourcePtr<IDXGISwapChain3> swapChain;
	size_t frame = 0;  // Stores the actual frame number. Refers to the current CPU frame being run, stepped after finishing CPU pass.

	size_t syncLastValue = 0;  // The most recently signalled value, from the CPU.
	std::vector<uint64_t> syncFenceValues;
	ResourcePtr<ID3D12Fence> syncFence;
	HANDLE syncEvent;

	ResourcePtr<D3D12MA::Allocator> allocator;
	ResourceManager resourceManager;

	DescriptorAllocator descriptorManager;

	std::array<TextureHandle, frameCount> backBufferTextures;  // Render targets bound to the swap chain.

	// Callbacks.
	ResourcePtr<ID3D12Fence> deviceRemovedFence;
	HANDLE deviceRemovedEvent;
	HANDLE deviceRemovedHandle;

	// Per-frame shared dynamic heap.
	std::array<BufferHandle, frameCount> frameBuffers;
	std::array<size_t, frameCount> frameBufferOffsets = {};

	// Per-frame command list pool.
	std::array<std::vector<std::shared_ptr<CommandList>>, frameCount> frameCommandLists;
	std::array<size_t, frameCount> frameCommandListOffsets = {};

	// Name the D3D objects.
	void SetNames();

	void SetupRenderTargets();

	// Resets command lists and allocators.
	void ResetFrame(size_t frameID);

public:
	RenderDevice(void* window, bool software, bool enableDebugging);
	~RenderDevice();

	auto* Native() const noexcept { return device.Get(); }

	// Logs various data about the device's feature support. Not needed in optimized builds.
	void CheckFeatureSupport();

	// Allocate a block of CPU write-only, GPU read-only memory from the per-frame dynamic heap.
	std::pair<BufferHandle, size_t> FrameAllocate(size_t size);

	// Allocate a per-frame command list, disposed of automatically.
	std::shared_ptr<CommandList> AllocateFrameCommandList(D3D12_COMMAND_LIST_TYPE type);

	DescriptorHandle AllocateDescriptor(DescriptorType type);

	// Fully sync the GPU, flushes all commands.
	void Synchronize();

	// This is a bit of a hack: when uploading large asset data, the relatively small frame upload heap
	// can get exhausted. To address this, when the buffer is full, flush all work and sync, then continue
	// the upload. The correct solution here is to use an async copy queue.
	void FlushUploadWork();

	void Present();

	void AdvanceCPU();  // Steps the CPU frame counter, blocking sync with GPU.
	void AdvanceGPU();  // Steps the GPU frame counter.
	size_t GetFrameIndex() const noexcept { return frame % RenderDevice::frameCount; }

	auto* GetDirectQueue() const noexcept { return directCommandQueue.Get(); }
	auto* GetDirectContext() const noexcept { return directContext; }
	auto& GetDirectList() noexcept { return directCommandList[GetFrameIndex()]; }

	auto* GetSwapChain() const noexcept { return swapChain.Get(); }
	auto GetBackBuffer() const noexcept { return backBufferTextures[swapChain->GetCurrentBackBufferIndex()]; }  // Resizing affects the buffer index, so use the swap chain's index.
	auto& GetDescriptorAllocator() noexcept { return descriptorManager; }
	auto& GetResourceManager() noexcept { return resourceManager; }

	void SetResolution(uint32_t width, uint32_t height, bool fullscreen);
};