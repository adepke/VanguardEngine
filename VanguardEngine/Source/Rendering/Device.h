// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Resource.h>
#include <Rendering/Adapter.h>  // #TODO: Including this before D3D12MemAlloc.h causes an array of errors, this needs to be fixed.
#include <Rendering/ResourceManager.h>
#include <Rendering/PipelineState.h>
#include <Rendering/DescriptorAllocator.h>
#include <Rendering/CommandList.h>
#include <Rendering/Material.h>

#include <D3D12MemAlloc.h>

#include <memory>
#include <string_view>
#include <array>
#include <utility>
#include <vector>
#include <limits>

struct Buffer;
struct Texture;
struct BufferDescription;
struct TextureDescription;

enum class SyncType
{
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
	const D3D_FEATURE_LEVEL TargetFeatureLevel = D3D_FEATURE_LEVEL_12_1;
	const D3D_SHADER_MODEL TargetShaderModel = D3D_SHADER_MODEL_6_3;

	// #NOTE: Ordering of these variables is significant for proper destruction!
	ResourcePtr<ID3D12Device3> Device;
	Adapter RenderAdapter;
	HWND WindowHandle;

	ResourcePtr<ID3D12CommandQueue> DirectCommandQueue;
	CommandList DirectCommandList[FrameCount];  // #TODO: One per worker thread.

	ResourcePtr<ID3D12CommandQueue> ComputeCommandQueue;
	CommandList ComputeCommandList[FrameCount];  // #TODO: One per worker thread.

	ResourcePtr<IDXGISwapChain3> SwapChain;
	size_t Frame = 0;  // Stores the actual frame number. Refers to the current CPU frame being run, stepped after finishing CPU pass.

	std::array<std::shared_ptr<Texture>, FrameCount> BackBufferTextures;  // Render targets bound to the swap chain.

	size_t IntraSyncValue = 0;  // Value to ensure that we signal a unique value every time we intra sync.
	ResourcePtr<ID3D12Fence> InterSyncFence;
	ResourcePtr<ID3D12Fence> IntraSyncFence;
	HANDLE SyncEvent;  // Shared event for inter and intra syncing.

	ResourcePtr<D3D12MA::Allocator> Allocator;
	ResourceManager AllocatorManager;

	std::array<std::shared_ptr<Buffer>, FrameCount> FrameBuffers;  // Per-frame shared dynamic heap.
	std::array<size_t, FrameCount> FrameBufferOffsets = {};

	DescriptorAllocator DescriptorManager;

	// #TODO: Don't use shared_ptr's here.
	std::array<std::vector<std::shared_ptr<CommandList>>, FrameCount> FrameCommandLists;  // Per-frame dynamic command lists.

	// Name the D3D objects.
	void SetNames();

	void SetupRenderTargets();

	// Resets command lists and allocators.
	void ResetFrame(size_t FrameID);

public:
	RenderDevice(HWND InWindow, bool Software, bool EnableDebugging);
	~RenderDevice();

	auto* Native() const noexcept { return Device.Get(); }

	// Logs various data about the device's feature support. Not needed in optimized builds.
	void CheckFeatureSupport();

	// Compiles shaders, builds pipelines.
	std::vector<Material> ReloadMaterials();

	std::shared_ptr<Buffer> CreateResource(const BufferDescription& Description, const std::wstring_view Name);
	std::shared_ptr<Texture> CreateResource(const TextureDescription& Description, const std::wstring_view Name);
	void WriteResource(std::shared_ptr<Buffer>& Target, const std::vector<uint8_t>& Source, size_t TargetOffset = 0);
	void WriteResource(std::shared_ptr<Texture>& Target, const std::vector<uint8_t>& Source);

	// Allocate a block of CPU write-only, GPU read-only memory from the per-frame dynamic heap.
	std::pair<std::shared_ptr<Buffer>, size_t> FrameAllocate(size_t Size);

	// Allocate a per-frame command list, disposed of automatically.
	std::shared_ptr<CommandList> AllocateFrameCommandList(D3D12_COMMAND_LIST_TYPE Type);

	DescriptorHandle AllocateDescriptor(DescriptorType Type);

	// Sync the GPU until it is either fully caught up or within the max buffered frames limit, determined by FullSync. Blocking.
	void SyncInterframe(bool FullSync);
	// Sync the specified GPU engine within the active frame on the GPU. Blocking.
	void SyncIntraframe(SyncType Type);

	void AdvanceCPU();  // Steps the CPU frame counter, blocking sync with GPU.
	void AdvanceGPU();  // Steps the GPU frame counter.
	size_t GetFrameIndex() const noexcept { return Frame % RenderDevice::FrameCount; }

	auto GetWindowHandle() const noexcept { return WindowHandle; }
	auto* GetDirectQueue() const noexcept { return DirectCommandQueue.Get(); }
	auto& GetDirectList() noexcept { return DirectCommandList[GetFrameIndex()]; }
	auto* GetComputeQueue() const noexcept { return ComputeCommandQueue.Get(); }
	auto& GetComputeList() noexcept { return ComputeCommandList[GetFrameIndex()]; }
	auto* GetSwapChain() const noexcept { return SwapChain.Get(); }
	auto GetBackBuffer() const noexcept { return BackBufferTextures[GetFrameIndex()]; }
	auto& GetDescriptorAllocator() noexcept { return DescriptorManager; }

	void SetResolution(size_t Width, size_t Height, bool InFullscreen);
};