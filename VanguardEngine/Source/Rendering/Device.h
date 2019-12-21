// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <D3D12MemAlloc.h>

#include <d3d12.h>
#include <dxgi1_4.h>

class ResourceManager;

class RenderDevice
{
private:
	bool Debugging = false;
	uint32_t FrameCount = 2;
	bool VSync = false;
	size_t RenderWidth = 10;
	size_t RenderHeight = 10;
	bool Fullscreen = false;

	const D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	// #NOTE: Ordering of these variables is significant for proper destruction!
	ResourcePtr<ID3D12Device> Device;
	ResourcePtr<ID3D12CommandAllocator> CommandAllocator;
	ResourcePtr<ID3D12CommandQueue> CommandQueue;
	ResourcePtr<IDXGISwapChain3> SwapChain;
	ResourcePtr<ID3D12Fence> FrameFence;
	HANDLE FrameFenceEvent;

	ResourcePtr<D3D12MA::Allocator> Allocator;

	ResourcePtr<IDXGIAdapter1> GetAdapter(ResourcePtr<IDXGIFactory4>& Factory, bool Software);

public:
	RenderDevice(HWND InWindow, bool Software, bool EnableDebugging);
	~RenderDevice();

	void SetResolution(size_t Width, size_t Height, bool InFullscreen);
};