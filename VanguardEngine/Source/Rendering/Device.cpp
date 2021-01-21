// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/Device.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/Material.h>
#include <Core/Config.h>

#include <algorithm>

#if !BUILD_RELEASE
#include <dxgidebug.h>
#endif

void RenderDevice::SetNames()
{
#if !BUILD_RELEASE
	VGScopedCPUStat("Device Set Names");

	device->SetName(VGText("Primary render device"));

	directCommandQueue->SetName(VGText("Direct command queue"));
	for (uint32_t i = 0; i < frameCount; ++i)
	{
		directCommandList[i].SetName(VGText("Direct command list"));
	}
#endif
}

void RenderDevice::SetupRenderTargets()
{
	VGScopedCPUStat("Setup Render Targets");

	HRESULT result;

	for (uint32_t i = 0; i < frameCount; ++i)
	{
		ID3D12Resource* intermediateResource;

		result = swapChain->GetBuffer(i, IID_PPV_ARGS(&intermediateResource));
		if (FAILED(result))
		{
			VGLogFatal(Rendering) << "Failed to get swap chain buffer for frame " << i << ": " << result;
		}

		backBufferTextures[i] = resourceManager.CreateFromSwapChain(static_cast<void*>(intermediateResource), VGText("Back buffer"));
	}
}

void RenderDevice::ResetFrame(size_t frameID)
{
	VGScopedCPUStat("Reset Frame");

	const auto frameIndex = frameID % frameCount;

	const auto result = directCommandList[frameIndex].Reset();
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to reset direct command list for frame " << frameIndex << ": " << result;
	}

	descriptorManager.FrameStep(frameIndex);

	for (auto& list : frameCommandLists[frameIndex])
	{
		list->Reset();
	}

	// #TODO: Don't discard the lists, they can be reused.
	frameCommandLists[frameIndex].clear();
}

RenderDevice::RenderDevice(void* window, bool software, bool enableDebugging)
{
	VGScopedCPUStat("Render Device Initialize");

	debugging = enableDebugging;

#if !BUILD_RELEASE
	if (enableDebugging)
	{
		VGScopedCPUStat("Render Device Enable Debug Layer");

		ResourcePtr<ID3D12Debug1> debugController;

		auto result = D3D12GetDebugInterface(IID_PPV_ARGS(debugController.Indirect()));
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Failed to get D3D12 debug interface: " << result;
		}

		else
		{
			debugController->EnableDebugLayer();

			// Limit GPU-based validation to debug builds only, as it has far greater performance impacts than
			// the debug layer. Development builds shouldn't be slowed down by this.
#if BUILD_DEBUG
			debugController->SetEnableGPUBasedValidation(true);
#endif
		}

		ResourcePtr<IDXGIInfoQueue> infoQueue;

		result = DXGIGetDebugInterface1(0, IID_PPV_ARGS(infoQueue.Indirect()));
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Failed to get DXGI info queue: " << result;
		}

		else
		{
			infoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
			infoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
		}
	}
#endif

	ResourcePtr<IDXGIFactory7> factory;
	uint32_t factoryFlags = 0;

	if (enableDebugging)
	{
		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	auto result = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create render device factory: " << result;
	}

	renderAdapter.Initialize(factory, targetFeatureLevel, software);

	Microsoft::WRL::ComPtr<ID3D12Device3> deviceCom;

	result = D3D12CreateDevice(renderAdapter.Native(), targetFeatureLevel, IID_PPV_ARGS(deviceCom.GetAddressOf()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create render device: " << result;
	}

#if !BUILD_RELEASE
	// Now that the device has been created, we can get the info queue and enable D3D12 message breaking.
	if (enableDebugging)
	{
		Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;

		result = deviceCom.As(&infoQueue);
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Failed to get D3D12 info queue: " << result;
		}
		
		else
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		}
	}
#endif

	device.Reset(deviceCom.Detach());

	D3D12MA::ALLOCATOR_DESC allocatorDesc{};
	allocatorDesc.pAdapter = renderAdapter.Native();
	allocatorDesc.pDevice = device.Get();
	allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;

	result = D3D12MA::CreateAllocator(&allocatorDesc, allocator.Indirect());
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create device allocator: " << result;
	}

	resourceManager.Initialize(this, frameCount);

	descriptorManager.Initialize(this, 2048, 64, 16);

	// #TODO: Condense building queues and command lists into a function.

	// Direct

	D3D12_COMMAND_QUEUE_DESC directCommandQueueDesc{};
	directCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	directCommandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	directCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	directCommandQueueDesc.NodeMask = 0;

	result = device->CreateCommandQueue(&directCommandQueueDesc, IID_PPV_ARGS(directCommandQueue.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create direct command queue: " << result;
	}

	directContext = TracyD3D12Context(device.Get(), directCommandQueue.Get());

	for (int i = 0; i < frameCount; ++i)
	{
		directCommandList[i].Create(this, D3D12_COMMAND_LIST_TYPE_DIRECT);

		// Close all lists except the current frame's list.
		if (i > 0)
		{
			directCommandList[i].Close();
		}
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDescription{};
	swapChainDescription.Width = renderWidth;
	swapChainDescription.Height = renderHeight;
	swapChainDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // Non-HDR. #TODO: Support HDR.
	swapChainDescription.BufferCount = frameCount;
	swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDescription.SampleDesc.Count = 1;
	swapChainDescription.SampleDesc.Quality = 0;
	swapChainDescription.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDescription.Stereo = false;
	swapChainDescription.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;  // #TODO: Check for tearing support before enabling.

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFSDescription{};
	swapChainFSDescription.RefreshRate.Numerator = 60;  // #TODO: Determine this based on the current monitor refresh rate?
	swapChainFSDescription.RefreshRate.Denominator = 1;
	swapChainFSDescription.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;  // Required for proper scaling.
	swapChainFSDescription.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	swapChainFSDescription.Windowed = !fullscreen;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChainWrapper;
	result = factory->CreateSwapChainForHwnd(directCommandQueue.Get(), static_cast<HWND>(window), &swapChainDescription, &swapChainFSDescription, nullptr, &swapChainWrapper);
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create swap chain: " << result;
	}

	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChainWrapperConverted;
	swapChainWrapper.As(&swapChainWrapperConverted);
	swapChain.Reset(swapChainWrapperConverted.Detach());

	result = factory->MakeWindowAssociation(static_cast<HWND>(window), DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to bind device to window: " << result;
	}

	syncValues.resize(frameCount, 0);

	result = device->CreateFence(syncValues[GetFrameIndex()], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(syncFence.Indirect()));
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to create sync fence: " << result;
	}

	++syncValues[GetFrameIndex()];

	if (syncEvent = ::CreateEvent(nullptr, false, false, VGText("Sync fence event")); !syncEvent)
	{
		VGLogFatal(Rendering) << "Failed to create sync event: " << GetPlatformError();
	}

	constexpr auto frameBufferSize = 1024 * 64;

	// Allocate frame buffers.
	for (uint32_t i = 0; i < frameCount; ++i)
	{
		BufferDescription description{};
		description.size = frameBufferSize;
		description.stride = 1;
		description.updateRate = ResourceFrequency::Dynamic;
		description.bindFlags = 0;
		description.accessFlags = AccessFlag::CPUWrite;

		frameBuffers[i] = resourceManager.Create(description, VGText("Frame buffer"));
	}

	SetupRenderTargets();

	SetNames();
}

RenderDevice::~RenderDevice()
{
	VGScopedCPUStat("Render Device Shutdown");

	Synchronize();

	::CloseHandle(syncEvent);

#if !BUILD_RELEASE
	if (debugging)
	{
		ResourcePtr<IDXGIDebug1> dxgiDebug;

		auto result = DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiDebug.Indirect()));
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Failed to get DXGI debug interface: " << result;
		}

		else
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS{ DXGI_DEBUG_RLO_ALL | DXGI_DEBUG_RLO_IGNORE_INTERNAL });
		}
	}
#endif
}

void RenderDevice::CheckFeatureSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
	auto result = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to check feature support for category 'options': " << result;
	}

	else
	{
		switch (options.ResourceBindingTier)
		{
		case D3D12_RESOURCE_BINDING_TIER_1: VGLog(Rendering) << "Device supports resource binding tier 1."; break;
		case D3D12_RESOURCE_BINDING_TIER_2: VGLog(Rendering) << "Device supports resource binding tier 2."; break;
		case D3D12_RESOURCE_BINDING_TIER_3: VGLog(Rendering) << "Device supports resource binding tier 3."; break;
		default:
			if (options.ResourceBindingTier > D3D12_RESOURCE_BINDING_TIER_3)
			{
				VGLog(Rendering) << "Device supports resource binding tier newer than 3.";
			}

			else
			{
				VGLogWarning(Rendering) << "Unable to determine device resource binding tier.";
			}
		}
	}

	D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels{ 1, &targetFeatureLevel };
	result = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to check feature support for category 'feature levels': " << result;
	}

	else
	{
		switch (featureLevels.MaxSupportedFeatureLevel)
		{
		case D3D_FEATURE_LEVEL_11_0: VGLog(Rendering) << "Device supports feature level 11.0."; break;
		case D3D_FEATURE_LEVEL_11_1: VGLog(Rendering) << "Device supports feature level 11.1."; break;
		case D3D_FEATURE_LEVEL_12_0: VGLog(Rendering) << "Device supports feature level 12.0."; break;
		case D3D_FEATURE_LEVEL_12_1: VGLog(Rendering) << "Device supports feature level 12.1."; break;
		default:
			if (featureLevels.MaxSupportedFeatureLevel < D3D_FEATURE_LEVEL_11_0)
			{
				VGLog(Rendering) << "Device supports feature level prior to 11.0.";
			}

			else
			{
				VGLog(Rendering) << "Device supports feature level newer than 12.1.";
			}
		}
	}

	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{ D3D_SHADER_MODEL_6_5 };  // Highest shader model available.
	result = device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to check feature support for category 'shader model': " << result;
	}

	else
	{
		switch (shaderModel.HighestShaderModel)
		{
		case D3D_SHADER_MODEL_5_1: VGLog(Rendering) << "Device supports shader model 5.1."; break;
		case D3D_SHADER_MODEL_6_0: VGLog(Rendering) << "Device supports shader model 6.0."; break;
		case D3D_SHADER_MODEL_6_1: VGLog(Rendering) << "Device supports shader model 6.1."; break;
		case D3D_SHADER_MODEL_6_2: VGLog(Rendering) << "Device supports shader model 6.2."; break;
		case D3D_SHADER_MODEL_6_3: VGLog(Rendering) << "Device supports shader model 6.3."; break;
		case D3D_SHADER_MODEL_6_4: VGLog(Rendering) << "Device supports shader model 6.4."; break;
		case D3D_SHADER_MODEL_6_5: VGLog(Rendering) << "Device supports shader model 6.5."; break;
		default:
			if (shaderModel.HighestShaderModel > D3D_SHADER_MODEL_6_5)
			{
				VGLog(Rendering) << "Device supports shader model newer than 6.5.";
			}

			else
			{
				VGLogWarning(Rendering) << "Unable to determine device shader model support.";
			}
		}
	}

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignature{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
	result = device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSignature, sizeof(rootSignature));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to check feature support for category 'root signature': " << result;
	}

	else
	{
		switch (rootSignature.HighestVersion)
		{
		case D3D_ROOT_SIGNATURE_VERSION_1_0: VGLog(Rendering) << "Device supports root signature 1.0."; break;
		case D3D_ROOT_SIGNATURE_VERSION_1_1: VGLog(Rendering) << "Device supports root signature 1.1."; break;
		default:
			if (rootSignature.HighestVersion > D3D_ROOT_SIGNATURE_VERSION_1_1)
			{
				VGLog(Rendering) << "Device supports root signature newer than 1.1.";
			}

			else
			{
				VGLogWarning(Rendering) << "Unable to determine device root signature support.";
			}
		}
	}
}

std::pair<BufferHandle, size_t> RenderDevice::FrameAllocate(size_t size)
{
	const auto frameIndex = frame % frameCount;

	frameBufferOffsets[frameIndex] += size;

	return { frameBuffers[frameIndex], frameBufferOffsets[frameIndex] - size };
}

std::shared_ptr<CommandList> RenderDevice::AllocateFrameCommandList(D3D12_COMMAND_LIST_TYPE type)
{
	const auto frameIndex = GetFrameIndex();
	const auto size = frameCommandLists[frameIndex].size();

	frameCommandLists[frameIndex].emplace_back(std::make_shared<CommandList>());
	frameCommandLists[frameIndex][size]->Create(this, type);
	frameCommandLists[frameIndex][size]->SetName(VGText("Frame command list"));

	return frameCommandLists[frameIndex][size];
}

DescriptorHandle RenderDevice::AllocateDescriptor(DescriptorType type)
{
	return descriptorManager.Allocate(type);
}

void RenderDevice::Synchronize()
{
	VGScopedCPUStat("Synchronize");

	// Dispatch a signal at the end of the command queue. This will ensure all commands
	// have finished execution.
	auto result = directCommandQueue->Signal(syncFence.Get(), syncValues[GetFrameIndex()]);
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to signal the sync fence during synchronization: " << result;
	}

	result = syncFence->SetEventOnCompletion(syncValues[GetFrameIndex()], syncEvent);
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to set fence completion event during synchronization: " << result;
	}

	WaitForSingleObject(syncEvent, INFINITE);

	++syncValues[GetFrameIndex()];
}

void RenderDevice::AdvanceCPU()
{
	VGScopedCPUStat("CPU Frame Advance");

	// Make sure we don't get too many CPU frames ahead of the GPU.
	const auto fenceValue = syncValues[GetFrameIndex()];
	const auto nextFrameIndex = (frame + 1) % frameCount;

	auto result = directCommandQueue->Signal(syncFence.Get(), fenceValue);
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to signal the sync fence during CPU advance: " << result;
	}

	if (syncFence->GetCompletedValue() < syncValues[nextFrameIndex])
	{
		result = syncFence->SetEventOnCompletion(syncValues[nextFrameIndex], syncEvent);
		if (FAILED(result))
		{
			VGLogFatal(Rendering) << "Failed to set fence completion event during CPU advance: " << result;
		}

		VGScopedCPUStat("Wait for GPU");

		WaitForSingleObjectEx(syncEvent, INFINITE, false);
	}

	syncValues[nextFrameIndex] = fenceValue + 1;

	// The frame has finished, cleanup its resources. #TODO: Will leave additional GPU gaps if we're bottlenecking on the CPU, consider deferred cleanup?
	frameBufferOffsets[nextFrameIndex] = 0;  // GPU has fully consumed the frame resources, we can now reuse the buffer.
	resourceManager.CleanupFrameResources(frame + 1);
	ResetFrame(frame + 1);

	// #TODO: Check our CPU frame budget, try and get some additional work done if we have time?

	VGStatFrameCPU();  // Mark the new frame.
	++frame;

	// #TODO: We should probably be using the swap chain's back buffer index, however we mismatch when resizing the
	// swap chain. Doesn't seem to be a problem for now, so leave it as is.
	//VGAssert(GetFrameIndex() == swapChain->GetCurrentBackBufferIndex(), "Mismatched swap chain frame index.");
}

void RenderDevice::AdvanceGPU()
{
	VGScopedCPUStat("GPU Frame Advance");
	VGStatFrameGPU(directContext);
}

void RenderDevice::SetResolution(uint32_t width, uint32_t height, bool inFullscreen)
{
	VGScopedCPUStat("Render Device Change Resolution");

	Synchronize();

	// Reset the sync values to the active sync value for this frame.
	for (auto& syncValue : syncValues)
	{
		syncValue = syncValues[GetFrameIndex()];
	}

	renderWidth = width;
	renderHeight = height;
	fullscreen = inFullscreen;

	// #TODO: Fullscreen.

	// Release the swap chain frame surfaces.
	for (const auto texture : backBufferTextures)
	{
		resourceManager.Destroy(texture);
	}

	// #TODO: Don't duplicate flags, use the original swap chain flags.
	auto result = swapChain->ResizeBuffers(static_cast<UINT>(frameCount), static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
	if (FAILED(result))
	{
		VGLogFatal(Rendering) << "Failed to resize swap chain buffers: " << result;
	}

	SetupRenderTargets();
}