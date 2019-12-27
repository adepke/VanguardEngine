// Copyright (c) 2019 Andrew Depke

#include <Rendering/Device.h>
#include <Rendering/ResourceManager.h>

#include <wrl/client.h>

ResourcePtr<IDXGIAdapter1> RenderDevice::GetAdapter(ResourcePtr<IDXGIFactory4>& Factory, bool Software)
{
	VGScopedCPUStat("Render Device Get Adapter");
	
	ResourcePtr<IDXGIAdapter1> Adapter;

	for (uint32_t Index = 0; Factory->EnumAdapters1(Index, Adapter.Indirect()) != DXGI_ERROR_NOT_FOUND; ++Index)
	{
		DXGI_ADAPTER_DESC1 Description;
		Adapter->GetDesc1(&Description);

		if (Software)
		{
			if ((Description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}
		}

		else
		{
			if ((Description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}
		}

		// Check the device and feature level without creating it.
		const auto CreateDeviceResult = D3D12CreateDevice(Adapter.Get(), FeatureLevel, __uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(CreateDeviceResult))
		{
			break;
		}
	}

	return std::move(Adapter);
}

void RenderDevice::WaitForFrame(size_t FrameID)
{
	VGScopedCPUStat("Wait For Frame");

	const auto FrameIndex = FrameID % FrameCount;

	auto Result = DirectCommandQueue->Signal(FrameFence.Get(), FrameIndex);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to submit signal command to GPU during frame wait: " << Result;
	}

	if (FrameFence->GetCompletedValue() != FrameIndex)
	{
		Result = FrameFence->SetEventOnCompletion(FrameIndex, FrameFenceEvent);
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to set fence completion event during frame wait: " << Result;
		}

		WaitForSingleObject(FrameFenceEvent, INFINITE);
	}
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

	ResourcePtr<IDXGIFactory4> Factory;
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

	auto Adapter{ GetAdapter(Factory, Software) };
	VGEnsure(Adapter.Get(), "Failed to find an adapter.");

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

	// Copy

	Result = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(CopyCommandAllocator.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create copy command allocator: " << Result;
	}

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

	Result = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, CopyCommandAllocator.Get(), nullptr, IID_PPV_ARGS(CopyCommandList.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create copy command list: " << Result;
	}

	// Direct

	Result = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(DirectCommandAllocator.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create direct command allocator: " << Result;
	}

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

	Result = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, DirectCommandAllocator.Get(), nullptr, IID_PPV_ARGS(DirectCommandList.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create direct command list: " << Result;
	}

	// Compute

	Result = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(ComputeCommandAllocator.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create compute command allocator: " << Result;
	}

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

	Result = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, ComputeCommandAllocator.Get(), nullptr, IID_PPV_ARGS(ComputeCommandList.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create compute command list: " << Result;
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

	Result = Device->CreateFence(Frame, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(FrameFence.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create frame fence: " << Result;
	}

	if (FrameFenceEvent = ::CreateEvent(nullptr, false, false, VGText("Frame Fence Event")); !FrameFenceEvent)
	{
		VGLogFatal(Rendering) << "Failed to create frame fence event: " << GetPlatformError();
	}
}

RenderDevice::~RenderDevice()
{
	VGScopedCPUStat("Render Device Shutdown");

	WaitForFrame(Frame);

	::CloseHandle(FrameFenceEvent);
}

std::shared_ptr<GPUBuffer> RenderDevice::Allocate(const ResourceDescription& Description, const std::wstring_view Name)
{
	return std::move(ResourceManager::Get().Allocate(*this, Allocator, Description, Name));
}

void RenderDevice::Write(std::shared_ptr<GPUBuffer>& Buffer, std::unique_ptr<ResourceWriteType>&& Source, size_t BufferOffset /*= 0*/)
{
	ResourceManager::Get().Write(*this, Buffer, std::forward<std::unique_ptr<ResourceWriteType>>(Source), BufferOffset);
}

void RenderDevice::FrameStep()
{
	WaitForFrame(Frame + 1);

	// The frame has finished, cleanup its resources. #TODO: Will leave additional GPU gaps if we're bottlenecking on the CPU, consider deferred cleanup?
	ResourceManager::Get().CleanupFrameResources(Frame + 1);

	// #TODO: Check our CPU frame budget, try and get some additional work done if we have time?

	VGStatFrame;  // Mark the new frame.
	++Frame;
}

void RenderDevice::SetResolution(size_t Width, size_t Height, bool InFullscreen)
{
	VGScopedCPUStat("Render Device Change Resolution");
}