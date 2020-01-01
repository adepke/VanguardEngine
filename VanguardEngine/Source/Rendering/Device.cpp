// Copyright (c) 2019 Andrew Depke

#include <Rendering/Device.h>
#include <Rendering/ResourceManager.h>

#include <wrl/client.h>

ResourcePtr<IDXGIAdapter1> RenderDevice::GetAdapter(ResourcePtr<IDXGIFactory7>& Factory, bool Software)
{
	VGScopedCPUStat("Render Device Get Adapter");

	ResourcePtr<IDXGIAdapter1> Adapter;

	if (Software)
	{
		Factory->EnumWarpAdapter(IID_PPV_ARGS(Adapter.Indirect()));
	}

	else
	{
		for (uint32_t Index = 0; Factory->EnumAdapterByGpuPreference(Index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(Adapter.Indirect())) != DXGI_ERROR_NOT_FOUND; ++Index)
		{
			DXGI_ADAPTER_DESC1 Description;
			Adapter->GetDesc1(&Description);

			if ((Description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			// Check the device and feature level without creating it.
			const auto CreateDeviceResult = D3D12CreateDevice(Adapter.Get(), FeatureLevel, __uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(CreateDeviceResult))
			{
				break;
			}
		}
	}

	return std::move(Adapter);
}

void RenderDevice::SetNames()
{
	VGScopedCPUStat("Device Set Names");

	Device->SetName(VGText("Primary Render Device"));

	CopyCommandQueue->SetName(VGText("Copy Command Queue"));
	for (auto Index = 0; Index < FrameCount; ++Index)
	{
		CopyCommandAllocator[Index]->SetName(VGText("Copy Command Allocator"));
		CopyCommandList[Index]->SetName(VGText("Copy Command List"));
	}

	DirectCommandQueue->SetName(VGText("Direct Command Queue"));
	for (auto Index = 0; Index < FrameCount; ++Index)
	{
		DirectCommandAllocator[Index]->SetName(VGText("Direct Command Allocator"));
		DirectCommandList[Index]->SetName(VGText("Direct Command List"));
	}

	ComputeCommandQueue->SetName(VGText("Compute Command Queue"));
	for (auto Index = 0; Index < FrameCount; ++Index)
	{
		ComputeCommandAllocator[Index]->SetName(VGText("Compute Command Allocator"));
		ComputeCommandList[Index]->SetName(VGText("Compute Command List"));
	}

	for (auto Index = 0; Index < FrameCount; ++Index)
	{
		ResourceHeaps[Index]->SetName(VGText("Resource Heap"));
		SamplerHeaps[Index]->SetName(VGText("Sampler Heap"));
	}

	RenderTargetHeap->SetName(VGText("Render Target Heap"));
	DepthStencilHeap->SetName(VGText("Depth Stencil Heap"));
}

void RenderDevice::SetupDescriptorHeaps()
{
	VGScopedCPUStat("Setup Descriptor Heaps");

	for (auto Index = 0; Index < FrameCount; ++Index)
	{

	}
}

void RenderDevice::SetupRenderTargets()
{
	VGScopedCPUStat("Setup Render Targets");

	HRESULT Result;

	for (auto Index = 0; Index < FrameCount; ++Index)
	{
		Result = SwapChain->GetBuffer(Index, IID_PPV_ARGS(FinalRenderTargets[Index].Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to get swap chain buffer for frame " << Index << ": " << Result;
		}

		// #TODO: Create rtv.
		//Device->CreateRenderTargetView(FinalRenderTargets[Index], nullptr, )
	}
}

void RenderDevice::ReloadShaders()
{
	VGScopedCPUStat("Reload Shaders");
}

void RenderDevice::ResetFrame(size_t FrameID)
{
	VGScopedCPUStat("Reset Frame");

	const auto FrameIndex = FrameID % FrameCount;

	auto Result = CopyCommandAllocator[FrameIndex]->Reset();
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset copy command allocator for frame " << FrameIndex << ": " << Result;
	}

	Result = static_cast<ID3D12GraphicsCommandList*>(CopyCommandList[FrameIndex].Get())->Reset(CopyCommandAllocator[FrameIndex].Get(), nullptr);
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset copy command list for frame " << FrameIndex << ": " << Result;
	}

	Result = DirectCommandAllocator[FrameIndex]->Reset();
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset direct command allocator for frame " << FrameIndex << ": " << Result;
	}

	Result = static_cast<ID3D12GraphicsCommandList*>(DirectCommandList[FrameIndex].Get())->Reset(DirectCommandAllocator[FrameIndex].Get(), nullptr);
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset direct command list for frame " << FrameIndex << ": " << Result;
	}

	// #TODO: Implement compute.
	/*
	Result = ComputeCommandAllocator[FrameIndex]->Reset();
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset compute command allocator for frame " << FrameIndex << ": " << Result;
	}

	Result = static_cast<ID3D12GraphicsCommandList*>(ComputeCommandList[FrameIndex].Get())->Reset(ComputeCommandAllocator[FrameIndex].Get(), nullptr);
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset compute command list for frame " << FrameIndex << ": " << Result;
	}
	*/
}

RenderDevice::RenderDevice(HWND InWindow, bool Software, bool EnableDebugging)
{
	VGScopedCPUStat("Render Device Initialize");

	Debugging = EnableDebugging;

	if (EnableDebugging)
	{
		VGScopedCPUStat("Render Device Enable Debug Layer");

		ResourcePtr<ID3D12Debug> DebugController;

		auto Result = D3D12GetDebugInterface(IID_PPV_ARGS(DebugController.Indirect()));
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Failed to get debug interface: " << Result;
		}

		else
		{
			DebugController->EnableDebugLayer();
		}
	}

	ResourcePtr<IDXGIFactory7> Factory;
	uint32_t FactoryFlags = 0;

	if (EnableDebugging)
	{
		FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	auto Result = CreateDXGIFactory2(FactoryFlags, IID_PPV_ARGS(Factory.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create render device factory: " << Result;
	}

	Adapter = std::move(GetAdapter(Factory, Software));
	VGEnsure(Adapter.Get(), "Failed to find an adapter.");

	DXGI_ADAPTER_DESC1 AdapterDesc;
	Adapter->GetDesc1(&AdapterDesc);

	VGLog(Rendering) << "Using adapter: " << AdapterDesc.Description;

	// #TODO: Adapter events.
	//Adapter->RegisterVideoMemoryBudgetChangeNotificationEvent();
	//Adapter->RegisterHardwareContentProtectionTeardownStatusEvent();

	Result = D3D12CreateDevice(Adapter.Get(), FeatureLevel, IID_PPV_ARGS(Device.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create render device: " << Result;
	}

	D3D12MA::ALLOCATOR_DESC AllocatorDesc{};
	AllocatorDesc.pAdapter = Adapter.Get();
	AllocatorDesc.pDevice = Device.Get();
	AllocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;

	Result = D3D12MA::CreateAllocator(&AllocatorDesc, Allocator.Indirect());
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create device allocator: " << Result;
	}

	AllocatorManager.Initialize(*this, FrameCount);

	// Copy

	D3D12_COMMAND_QUEUE_DESC CopyCommandQueueDesc{};
	CopyCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	CopyCommandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	CopyCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	CopyCommandQueueDesc.NodeMask = 0;

	Result = Device->CreateCommandQueue(&CopyCommandQueueDesc, IID_PPV_ARGS(CopyCommandQueue.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create copy command queue: " << Result;
	}

	for (int Index = 0; Index < FrameCount; ++Index)
	{
		Result = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(CopyCommandAllocator[Index].Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create copy command allocator for frame " << Index << ": " << Result;
		}

		Result = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, CopyCommandAllocator[Index].Get(), nullptr, IID_PPV_ARGS(CopyCommandList[Index].Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create copy command list for frame " << Index << ": " << Result;
		}

		// Close all lists except the current frame's list.
		if (Index > 0)
		{
			static_cast<ID3D12GraphicsCommandList*>(CopyCommandList[Index].Get())->Close();
		}
	}

	// Direct

	D3D12_COMMAND_QUEUE_DESC DirectCommandQueueDesc{};
	DirectCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DirectCommandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	DirectCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	DirectCommandQueueDesc.NodeMask = 0;

	Result = Device->CreateCommandQueue(&DirectCommandQueueDesc, IID_PPV_ARGS(DirectCommandQueue.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create direct command queue: " << Result;
	}

	for (int Index = 0; Index < FrameCount; ++Index)
	{
		Result = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(DirectCommandAllocator[Index].Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create direct command allocator for frame " << Index << ": " << Result;
		}

		Result = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, DirectCommandAllocator[Index].Get(), nullptr, IID_PPV_ARGS(DirectCommandList[Index].Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create direct command list for frame " << Index << ": " << Result;
		}

		// Close all lists except the current frame's list.
		if (Index > 0)
		{
			static_cast<ID3D12GraphicsCommandList*>(DirectCommandList[Index].Get())->Close();
		}
	}

	// Compute

	D3D12_COMMAND_QUEUE_DESC ComputeCommandQueueDesc{};
	ComputeCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ComputeCommandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	ComputeCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ComputeCommandQueueDesc.NodeMask = 0;

	Result = Device->CreateCommandQueue(&ComputeCommandQueueDesc, IID_PPV_ARGS(ComputeCommandQueue.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create compute command queue: " << Result;
	}

	for (int Index = 0; Index < FrameCount; ++Index)
	{
		Result = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(ComputeCommandAllocator[Index].Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create compute command allocator for frame " << Index << ": " << Result;
		}

		Result = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, ComputeCommandAllocator[Index].Get(), nullptr, IID_PPV_ARGS(ComputeCommandList[Index].Indirect()));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to create compute command list for frame " << Index << ": " << Result;
		}

		// Close all lists except the current frame's list.
		if (Index > 0)
		{
			static_cast<ID3D12GraphicsCommandList*>(ComputeCommandList[Index].Get())->Close();
		}
	}

	DXGI_SWAP_CHAIN_DESC1 SwapChainDescription{};
	SwapChainDescription.Width = static_cast<UINT>(RenderWidth);
	SwapChainDescription.Height = static_cast<UINT>(RenderHeight);
	SwapChainDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDescription.BufferCount = FrameCount;
	SwapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDescription.SampleDesc.Count = 1;
	SwapChainDescription.SampleDesc.Quality = 0;
	SwapChainDescription.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	SwapChainDescription.Stereo = false;
	SwapChainDescription.Flags = 0;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC SwapChainFSDescription{};
	SwapChainFSDescription.RefreshRate.Numerator = 60;  // #TODO: Determine this based on the current monitor refresh rate?
	SwapChainFSDescription.RefreshRate.Denominator = 1;
	SwapChainFSDescription.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;  // Required for proper scaling.
	SwapChainFSDescription.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	SwapChainFSDescription.Windowed = !Fullscreen;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> SwapChainWrapper;
	Result = Factory->CreateSwapChainForHwnd(DirectCommandQueue.Get(), InWindow, &SwapChainDescription, &SwapChainFSDescription, nullptr, &SwapChainWrapper);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create swap chain: " << Result;
	}

	Microsoft::WRL::ComPtr<IDXGISwapChain3> SwapChainWrapperConverted;
	SwapChainWrapper.As(&SwapChainWrapperConverted);
	SwapChain.Reset(SwapChainWrapperConverted.Detach());

	Result = Factory->MakeWindowAssociation(InWindow, DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to bind device to window: " << Result;
	}

	Result = Device->CreateFence(Frame, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(CopyFence.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create copy fence: " << Result;
	}

	if (CopyFenceEvent = ::CreateEvent(nullptr, false, false, VGText("Copy Fence Event")); !CopyFenceEvent)
	{
		VGLogFatal(Rendering) << "Failed to create copy fence event: " << GetPlatformError();
	}

	Result = Device->CreateFence(Frame, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(DirectFence.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create direct fence: " << Result;
	}

	if (DirectFenceEvent = ::CreateEvent(nullptr, false, false, VGText("Direct Fence Event")); !DirectFenceEvent)
	{
		VGLogFatal(Rendering) << "Failed to create direct fence event: " << GetPlatformError();
	}

	Result = Device->CreateFence(Frame, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(ComputeFence.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create compute fence: " << Result;
	}

	if (ComputeFenceEvent = ::CreateEvent(nullptr, false, false, VGText("Compute Fence Event")); !ComputeFenceEvent)
	{
		VGLogFatal(Rendering) << "Failed to create compute fence event: " << GetPlatformError();
	}

	constexpr auto FrameBufferSize = 1024 * 1024 * 64;

	// Allocate frame buffers.
	for (auto Index = 0; Index < FrameCount; ++Index)
	{
		ResourceDescription Description{};
		Description.Size = FrameBufferSize;
		Description.Stride = 1;
		Description.UpdateRate = ResourceFrequency::Dynamic;
		Description.BindFlags = BindFlag::ConstantBuffer;  // #TODO: Correct?
		Description.AccessFlags = AccessFlag::CPUWrite;

		FrameBuffers[Index] = std::move(AllocatorManager.Allocate(*this, Description, VGText("Frame Buffer")));
	}

	SetupDescriptorHeaps();
	SetupRenderTargets();

	if (Debugging)
	{
		SetNames();
	}
}

RenderDevice::~RenderDevice()
{
	VGScopedCPUStat("Render Device Shutdown");

	Sync(SyncType::Direct, Frame);

	::CloseHandle(CopyFenceEvent);
	::CloseHandle(DirectFenceEvent);
	::CloseHandle(ComputeFenceEvent);
}

std::shared_ptr<GPUBuffer> RenderDevice::Allocate(const ResourceDescription& Description, const std::wstring_view Name)
{
	return std::move(AllocatorManager.Allocate(*this, Description, Name));
}

void RenderDevice::Write(std::shared_ptr<GPUBuffer>& Buffer, const std::vector<uint8_t>& Source, size_t BufferOffset)
{
	AllocatorManager.Write(*this, Buffer, Source, BufferOffset);
}

std::pair<std::shared_ptr<GPUBuffer>, size_t> RenderDevice::FrameAllocate(size_t Size)
{
	const auto FrameIndex = Frame % FrameCount;

	FrameBufferOffsets[FrameIndex] += Size;

	return { FrameBuffers[FrameIndex], FrameBufferOffsets[FrameIndex] - Size };
}

void RenderDevice::Sync(SyncType Type, size_t FrameID)
{
	VGScopedCPUStat("Render Device Sync");

	ID3D12CommandQueue* SyncQueue = nullptr;
	ID3D12Fence* SyncFence = nullptr;
	HANDLE SyncEvent = nullptr;

	switch (Type)
	{
	case SyncType::Copy:
		SyncQueue = CopyCommandQueue.Get();
		SyncFence = CopyFence.Get();
		SyncEvent = CopyFenceEvent;
		break;
	case SyncType::Direct:
		SyncQueue = DirectCommandQueue.Get();
		SyncFence = DirectFence.Get();
		SyncEvent = DirectFenceEvent;
		break;
	case SyncType::Compute:
		SyncQueue = ComputeCommandQueue.Get();
		SyncFence = ComputeFence.Get();
		SyncEvent = ComputeFenceEvent;
		break;
	}

	const auto FrameIndex = FrameID % FrameCount;

	auto Result = SyncQueue->Signal(SyncFence, FrameIndex);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to submit signal command to GPU during sync: " << Result;
	}

	if (SyncFence->GetCompletedValue() != FrameIndex)
	{
		Result = SyncFence->SetEventOnCompletion(FrameIndex, SyncEvent);
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to set fence completion event during sync: " << Result;
		}

		WaitForSingleObject(SyncEvent, INFINITE);
	}
}

void RenderDevice::FrameStep()
{
	VGScopedCPUStat("Frame Step");

	Sync(SyncType::Direct, Frame + 1);

	FrameBufferOffsets[(Frame + 1) % FrameCount] = 0;  // GPU has fully consumed the frame resources, we can now reuse the buffer.

	// The frame has finished, cleanup its resources. #TODO: Will leave additional GPU gaps if we're bottlenecking on the CPU, consider deferred cleanup?
	AllocatorManager.CleanupFrameResources(Frame + 1);

	ResetFrame(Frame + 1);

	// #TODO: Check our CPU frame budget, try and get some additional work done if we have time?

	VGStatFrame;  // Mark the new frame.
	++Frame;
}

void RenderDevice::SetResolution(size_t Width, size_t Height, bool InFullscreen)
{
	VGScopedCPUStat("Render Device Change Resolution");

	Sync(SyncType::Direct, Frame);

	RenderWidth = Width;
	RenderHeight = Height;
	Fullscreen = InFullscreen;

	// #TODO: Fullscreen.

	auto Result = SwapChain->ResizeBuffers(FrameCount, Width, Height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to resize swap chain buffers: " << Result;
	}

	SetupRenderTargets();
}