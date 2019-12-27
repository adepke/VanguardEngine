// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Resource.h>

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

	static constexpr uint32_t FrameCount = 3;

private:
	const D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	// #NOTE: Ordering of these variables is significant for proper destruction!
	ResourcePtr<ID3D12Device> Device;

	ResourcePtr<ID3D12CommandAllocator> CopyCommandAllocator;  // #TODO: Probably per frame.
	ResourcePtr<ID3D12CommandQueue> CopyCommandQueue;
	ResourcePtr<ID3D12CommandList> CopyCommandList;

	ResourcePtr<ID3D12CommandAllocator> DirectCommandAllocator;  // #TODO: One per worker thread? One set per frame?
	ResourcePtr<ID3D12CommandQueue> DirectCommandQueue;  // #TODO: One per worker thread? One set per frame? We bind one to the window so maybe only 1?
	ResourcePtr<ID3D12GraphicsCommandList> DirectCommandList;  // #TODO: One per worker thread? One set per frame?

	ResourcePtr<ID3D12CommandAllocator> ComputeCommandAllocator;  // #TODO: One per worker thread? One set per frame?
	ResourcePtr<ID3D12CommandQueue> ComputeCommandQueue;  // #TODO: One per worker thread? One set per frame?
	ResourcePtr<ID3D12CommandList> ComputeCommandList;  // #TODO: One per worker thread? One set per frame?

	ResourcePtr<IDXGISwapChain3> SwapChain;
	size_t Frame = 0;  // Stores the actual frame number. Refers to the current CPU frame being run, stepped after finishing CPU pass.
	ResourcePtr<ID3D12Fence> FrameFence;
	HANDLE FrameFenceEvent;

	ResourcePtr<D3D12MA::Allocator> Allocator;

	ResourcePtr<IDXGIAdapter1> GetAdapter(ResourcePtr<IDXGIFactory4>& Factory, bool Software);

	// Blocking.
	void WaitForFrame(size_t FrameID);

public:
	RenderDevice(HWND InWindow, bool Software, bool EnableDebugging);
	~RenderDevice();

	std::shared_ptr<GPUBuffer> Allocate(const ResourceDescription& Description, const std::wstring_view Name);
	void Write(std::shared_ptr<GPUBuffer>& Buffer, std::unique_ptr<ResourceWriteType>&& Source, size_t BufferOffset = 0);

	// Blocking, waits for the gpu to finish the next frame before returning. Marks the current frame as finished submitting and can move on to the next frame.
	void FrameStep();

	void SetResolution(size_t Width, size_t Height, bool InFullscreen);
};