// Copyright (c) 2019 Andrew Depke

#include <Rendering/Device.h>

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

RenderDevice::RenderDevice(HWND InWindow, bool Software, bool EnableDebugging)
{
	VGScopedCPUStat("Render Device Initialize");

	if (EnableDebugging)
	{
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

	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc{};
	ZeroMemory(&CommandQueueDesc, sizeof(CommandQueueDesc));
	CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	Result = Device->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(CommandQueue.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create command queue: " << Result;
	}

	DXGI_SWAP_CHAIN_DESC1 SwapChainDescription{};
	ZeroMemory(&SwapChainDescription, sizeof(SwapChainDescription));
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
	ZeroMemory(&SwapChainFSDescription, sizeof(SwapChainFSDescription));
	SwapChainFSDescription.RefreshRate.Numerator = 60;  // #TODO: Determine this based on the current monitor refresh rate?
	SwapChainFSDescription.RefreshRate.Denominator = 1;
	SwapChainFSDescription.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;  // Required for proper scaling.
	SwapChainFSDescription.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	SwapChainFSDescription.Windowed = !Fullscreen;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> SwapChainWrapper;
	Result = Factory->CreateSwapChainForHwnd(CommandQueue.Get(), InWindow, &SwapChainDescription, &SwapChainFSDescription, nullptr, &SwapChainWrapper);
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
}

RenderDevice::~RenderDevice()
{
	VGScopedCPUStat("Render Device Shutdown");

	// #TODO: Full frame sync.
}

void RenderDevice::SetResolution(size_t Width, size_t Height, bool InFullscreen)
{
	VGScopedCPUStat("Render Device Change Resolution");
}