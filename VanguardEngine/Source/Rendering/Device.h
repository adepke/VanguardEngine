// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Resource.h>
#include <Rendering/ResourceManager.h>

#include <D3D12MemAlloc.h>

#include <memory>
#include <string_view>
#include <array>

#include <d3d12.h>
#include <dxgi1_4.h>

struct ResourceDescription;

class RenderDevice
{
	friend class Renderer;
	friend class ResourceManager;

public:
	bool Debugging = false;
	bool VSync = false;
	size_t RenderWidth = 10;
	size_t RenderHeight = 10;
	bool Fullscreen = false;

	static constexpr uint32_t FrameCount = 3;  // #TODO: Determine at runtime.

private:
	const D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	// #NOTE: Ordering of these variables is significant for proper destruction!
	ResourcePtr<ID3D12Device> Device;
	ResourcePtr<IDXGIAdapter1> Adapter;

	ResourcePtr<ID3D12CommandQueue> CopyCommandQueue;
	ResourcePtr<ID3D12CommandAllocator> CopyCommandAllocator[FrameCount];
	ResourcePtr<ID3D12CommandList> CopyCommandList[FrameCount];

	ResourcePtr<ID3D12CommandQueue> DirectCommandQueue;
	ResourcePtr<ID3D12CommandAllocator> DirectCommandAllocator[FrameCount];  // #TODO: One per worker thread.
	ResourcePtr<ID3D12GraphicsCommandList> DirectCommandList[FrameCount];  // #TODO: One per worker thread.

	ResourcePtr<ID3D12CommandQueue> ComputeCommandQueue;
	ResourcePtr<ID3D12CommandAllocator> ComputeCommandAllocator[FrameCount];  // #TODO: One per worker thread.
	ResourcePtr<ID3D12CommandList> ComputeCommandList[FrameCount];  // #TODO: One per worker thread.

	ResourcePtr<IDXGISwapChain3> SwapChain;
	size_t Frame = 0;  // Stores the actual frame number. Refers to the current CPU frame being run, stepped after finishing CPU pass.
	ResourcePtr<ID3D12Fence> FrameFence;
	HANDLE FrameFenceEvent;

	ResourcePtr<D3D12MA::Allocator> Allocator;
	ResourceManager AllocatorManager;

	ResourcePtr<IDXGIAdapter1> GetAdapter(ResourcePtr<IDXGIFactory4>& Factory, bool Software);

	// Name the D3D objects.
	void SetNames();

	// Blocking.
	void WaitForFrame(size_t FrameID);

	// Resets command lists and allocators.
	void ResetFrame(size_t FrameID);

public:
	RenderDevice(HWND InWindow, bool Software, bool EnableDebugging);
	~RenderDevice();

	std::shared_ptr<GPUBuffer> Allocate(const ResourceDescription& Description, const std::wstring_view Name);
	void Write(std::shared_ptr<GPUBuffer>& Buffer, const std::vector<uint8_t>& Source, size_t BufferOffset = 0);

	// Blocking, waits for the gpu to finish the next frame before returning. Marks the current frame as finished submitting and can move on to the next frame.
	void FrameStep();

	void SetResolution(size_t Width, size_t Height, bool InFullscreen);
};