// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/Device.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Rendering/Material.h>
#include <Core/Config.h>

#include <filesystem>
#include <algorithm>
#include <fstream>

#include <wrl/client.h>
#include <json.hpp>  // Needed for materials.

void RenderDevice::SetNames()
{
#if !BUILD_RELEASE
	VGScopedCPUStat("Device Set Names");

	Device->SetName(VGText("Primary Render Device"));

	CopyCommandQueue->SetName(VGText("Copy Command Queue"));
	for (uint32_t Index = 0; Index < FrameCount; ++Index)
	{
		CopyCommandList[Index].SetName(VGText("Copy Command List"));
	}

	DirectCommandQueue->SetName(VGText("Direct Command Queue"));
	for (uint32_t Index = 0; Index < FrameCount; ++Index)
	{
		DirectCommandList[Index].SetName(VGText("Direct Command List"));
	}

	ComputeCommandQueue->SetName(VGText("Compute Command Queue"));
	for (uint32_t Index = 0; Index < FrameCount; ++Index)
	{
		ComputeCommandList[Index].SetName(VGText("Compute Command List"));
	}

	CopyFence->SetName(VGText("Copy Fence"));
	DirectFence->SetName(VGText("Direct Fence"));
	ComputeFence->SetName(VGText("Compute Fence"));
#endif
}

void RenderDevice::SetupRenderTargets()
{
	VGScopedCPUStat("Setup Render Targets");

	HRESULT Result;

	for (uint32_t Index = 0; Index < FrameCount; ++Index)
	{
		ID3D12Resource* IntermediateResource;

		Result = SwapChain->GetBuffer(Index, IID_PPV_ARGS(&IntermediateResource));
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to get swap chain buffer for frame " << Index << ": " << Result;
		}

		BackBufferTextures[Index] = std::move(AllocatorManager.TextureFromSwapChain(static_cast<void*>(IntermediateResource), VGText("Back Buffer")));
	}
}

std::vector<Material> RenderDevice::ReloadMaterials()
{
	VGScopedCPUStat("Reload Materials");

	VGLog(Rendering) << "Reloading materials.";

	std::vector<Material> Materials;

	for (const auto& Entry : std::filesystem::directory_iterator{ Config::Get().MaterialsPath })
	{
		// #TODO: Move to standardized asset loading pipeline.

		std::ifstream MaterialStream{ Entry };

		if (!MaterialStream.is_open())
		{
			VGLogError(Rendering) << "Failed to open material asset at '" << Entry.path().generic_wstring() << "'.";
			continue;
		}

		nlohmann::json MaterialData;
		MaterialStream >> MaterialData;

		Material NewMat{};

		auto MaterialShaders = MaterialData["Shaders"].get<std::string>();
		NewMat.BackFaceCulling = MaterialData["BackFaceCulling"].get<bool>();

		PipelineStateDescription Desc{};
		Desc.ShaderPath = Config::Get().EngineRoot / MaterialShaders;
		Desc.BlendDescription.AlphaToCoverageEnable = false;
		Desc.BlendDescription.IndependentBlendEnable = false;
		Desc.BlendDescription.RenderTarget[0].BlendEnable = false;
		Desc.BlendDescription.RenderTarget[0].LogicOpEnable = false;
		Desc.BlendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		Desc.BlendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		Desc.BlendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		Desc.BlendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		Desc.BlendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		Desc.BlendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		Desc.BlendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		Desc.BlendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		Desc.RasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;  // #TODO: Support wire frame rendering.
		Desc.RasterizerDescription.CullMode = NewMat.BackFaceCulling ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
		Desc.RasterizerDescription.FrontCounterClockwise = false;
		Desc.RasterizerDescription.DepthBias = 0;
		Desc.RasterizerDescription.DepthBiasClamp = 0.f;
		Desc.RasterizerDescription.SlopeScaledDepthBias = 0.f;
		Desc.RasterizerDescription.DepthClipEnable = true;
		Desc.RasterizerDescription.MultisampleEnable = false;  // #TODO: Support multi-sampling.
		Desc.RasterizerDescription.AntialiasedLineEnable = false;  // #TODO: Support anti-aliasing.
		Desc.RasterizerDescription.ForcedSampleCount = 0;
		Desc.RasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		Desc.DepthStencilDescription.DepthEnable = true;
		Desc.DepthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		Desc.DepthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		Desc.DepthStencilDescription.StencilEnable = false;  // #TODO: Support stencil.
		Desc.DepthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		Desc.DepthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		Desc.DepthStencilDescription.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		Desc.DepthStencilDescription.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		Desc.DepthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		Desc.Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

		NewMat.Pipeline = std::make_unique<PipelineState>();
		NewMat.Pipeline->Build(*this, Desc);  // #TODO: Pipeline libraries.

		Materials.push_back(std::move(NewMat));
	}

	return std::move(Materials);
}

std::shared_ptr<Buffer> RenderDevice::CreateResource(const BufferDescription& Description, const std::wstring_view Name)
{
	return std::move(AllocatorManager.AllocateBuffer(Description, Name));
}

std::shared_ptr<Texture> RenderDevice::CreateResource(const TextureDescription& Description, const std::wstring_view Name)
{
	return std::move(AllocatorManager.AllocateTexture(Description, Name));
}

void RenderDevice::WriteResource(std::shared_ptr<Buffer>& Target, const std::vector<uint8_t>& Source, size_t TargetOffset)
{
	AllocatorManager.WriteBuffer(Target, Source, TargetOffset);
}

void RenderDevice::WriteResource(std::shared_ptr<Texture>& Target, const std::vector<uint8_t>& Source)
{
	AllocatorManager.WriteTexture(Target, Source);
}

void RenderDevice::ResetFrame(size_t FrameID)
{
	VGScopedCPUStat("Reset Frame");

	const auto FrameIndex = FrameID % FrameCount;

	auto Result = DirectToCopyCommandList[FrameIndex].Reset();
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset direct/compute to copy command list for frame " << FrameIndex << ": " << Result;
	}

	Result = CopyCommandList[FrameIndex].Reset();
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset copy command list for frame " << FrameIndex << ": " << Result;
	}

	Result = DirectCommandList[FrameIndex].Reset();
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to reset direct command list for frame " << FrameIndex << ": " << Result;
	}

	DescriptorManager.FrameStep(FrameIndex);
}

RenderDevice::RenderDevice(HWND InWindow, bool Software, bool EnableDebugging)
{
	VGScopedCPUStat("Render Device Initialize");

	WindowHandle = InWindow;
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

	RenderAdapter.Initialize(Factory, TargetFeatureLevel, Software);

	Result = D3D12CreateDevice(RenderAdapter.Native(), TargetFeatureLevel, IID_PPV_ARGS(Device.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create render device: " << Result;
	}

	D3D12MA::ALLOCATOR_DESC AllocatorDesc{};
	AllocatorDesc.pAdapter = RenderAdapter.Native();
	AllocatorDesc.pDevice = Device.Get();
	AllocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;

	Result = D3D12MA::CreateAllocator(&AllocatorDesc, Allocator.Indirect());
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create device allocator: " << Result;
	}

	AllocatorManager.Initialize(this, FrameCount);

	DescriptorManager.Initialize(this, 1024, 128);

	// #TODO: Condense building queues and command lists into a function.

	// Direct to Copy

	for (int Index = 0; Index < FrameCount; ++Index)
	{
		DirectToCopyCommandList[Index].Create(this, D3D12_COMMAND_LIST_TYPE_DIRECT);

		// Close all lists except the current frame's list.
		if (Index > 0)
		{
			DirectToCopyCommandList[Index].Close();
		}
	}

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
		CopyCommandList[Index].Create(this, D3D12_COMMAND_LIST_TYPE_COPY);

		// Close all lists except the current frame's list.
		if (Index > 0)
		{
			CopyCommandList[Index].Close();
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
		DirectCommandList[Index].Create(this, D3D12_COMMAND_LIST_TYPE_DIRECT);

		// Close all lists except the current frame's list.
		if (Index > 0)
		{
			DirectCommandList[Index].Close();
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
		ComputeCommandList[Index].Create(this, D3D12_COMMAND_LIST_TYPE_COMPUTE);

		// Close all lists except the current frame's list.
		if (Index > 0)
		{
			ComputeCommandList[Index].Close();
		}
	}

	DXGI_SWAP_CHAIN_DESC1 SwapChainDescription{};
	SwapChainDescription.Width = static_cast<UINT>(RenderWidth);
	SwapChainDescription.Height = static_cast<UINT>(RenderHeight);
	SwapChainDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // Non-HDR. #TODO: Support HDR.
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

	Result = Device->CreateFence(IntraSyncValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(IntraSyncFence.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create intra sync fence: " << Result;
	}

	if (IntraSyncEvent = ::CreateEvent(nullptr, false, false, VGText("Intra Sync Fence Event")); !IntraSyncEvent)
	{
		VGLogFatal(Rendering) << "Failed to create intra sync fence event: " << GetPlatformError();
	}

	constexpr auto FrameBufferSize = 1024 * 64;

	// Allocate frame buffers.
	for (uint32_t Index = 0; Index < FrameCount; ++Index)
	{
		BufferDescription Description{};
		Description.Size = FrameBufferSize;
		Description.Stride = 1;
		Description.UpdateRate = ResourceFrequency::Dynamic;
		Description.BindFlags = 0;
		Description.AccessFlags = AccessFlag::CPUWrite;

		FrameBuffers[Index] = std::move(AllocatorManager.AllocateBuffer(Description, VGText("Frame Buffer")));
	}

	SetupRenderTargets();

	SetNames();
}

RenderDevice::~RenderDevice()
{
	VGScopedCPUStat("Render Device Shutdown");

	SyncInterframe(true);

	::CloseHandle(CopyFenceEvent);
	::CloseHandle(DirectFenceEvent);
	::CloseHandle(ComputeFenceEvent);
	::CloseHandle(IntraSyncEvent);
}

void RenderDevice::CheckFeatureSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS Options{};
	auto Result = Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &Options, sizeof(Options));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to check feature support for category 'options': " << Result;
	}

	else
	{
		switch (Options.ResourceBindingTier)
		{
		case D3D12_RESOURCE_BINDING_TIER_1: VGLog(Rendering) << "Device supports resource binding tier 1."; break;
		case D3D12_RESOURCE_BINDING_TIER_2: VGLog(Rendering) << "Device supports resource binding tier 2."; break;
		case D3D12_RESOURCE_BINDING_TIER_3: VGLog(Rendering) << "Device supports resource binding tier 3."; break;
		default:
			if (Options.ResourceBindingTier > D3D12_RESOURCE_BINDING_TIER_3)
			{
				VGLog(Rendering) << "Device supports resource binding tier newer than 3.";
			}

			else
			{
				VGLogWarning(Rendering) << "Unable to determine device resource binding tier.";
			}
		}
	}

	D3D12_FEATURE_DATA_FEATURE_LEVELS FeatureLevels{ 1, &TargetFeatureLevel };
	Result = Device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &FeatureLevels, sizeof(FeatureLevels));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to check feature support for category 'feature levels': " << Result;
	}

	else
	{
		switch (FeatureLevels.MaxSupportedFeatureLevel)
		{
		case D3D_FEATURE_LEVEL_11_0: VGLog(Rendering) << "Device supports feature level 11.0."; break;
		case D3D_FEATURE_LEVEL_11_1: VGLog(Rendering) << "Device supports feature level 11.1."; break;
		case D3D_FEATURE_LEVEL_12_0: VGLog(Rendering) << "Device supports feature level 12.0."; break;
		case D3D_FEATURE_LEVEL_12_1: VGLog(Rendering) << "Device supports feature level 12.1."; break;
		default:
			if (FeatureLevels.MaxSupportedFeatureLevel < D3D_FEATURE_LEVEL_11_0)
			{
				VGLog(Rendering) << "Device supports feature level prior to 11.0.";
			}

			else
			{
				VGLog(Rendering) << "Device supports feature level newer than 12.1.";
			}
		}
	}

	D3D12_FEATURE_DATA_SHADER_MODEL ShaderModel{ D3D_SHADER_MODEL_6_5 };  // Highest shader model available.
	Result = Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &ShaderModel, sizeof(ShaderModel));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to check feature support for category 'shader model': " << Result;
	}

	else
	{
		switch (ShaderModel.HighestShaderModel)
		{
		case D3D_SHADER_MODEL_5_1: VGLog(Rendering) << "Device supports shader model 5.1."; break;
		case D3D_SHADER_MODEL_6_0: VGLog(Rendering) << "Device supports shader model 6.0."; break;
		case D3D_SHADER_MODEL_6_1: VGLog(Rendering) << "Device supports shader model 6.1."; break;
		case D3D_SHADER_MODEL_6_2: VGLog(Rendering) << "Device supports shader model 6.2."; break;
		case D3D_SHADER_MODEL_6_3: VGLog(Rendering) << "Device supports shader model 6.3."; break;
		case D3D_SHADER_MODEL_6_4: VGLog(Rendering) << "Device supports shader model 6.4."; break;
		case D3D_SHADER_MODEL_6_5: VGLog(Rendering) << "Device supports shader model 6.5."; break;
		default:
			if (ShaderModel.HighestShaderModel > D3D_SHADER_MODEL_6_5)
			{
				VGLog(Rendering) << "Device supports shader model newer than 6.5.";
			}

			else
			{
				VGLogWarning(Rendering) << "Unable to determine device shader model support.";
			}
		}
	}

	D3D12_FEATURE_DATA_ROOT_SIGNATURE RootSignature{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
	Result = Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &RootSignature, sizeof(RootSignature));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to check feature support for category 'root signature': " << Result;
	}

	else
	{
		switch (RootSignature.HighestVersion)
		{
		case D3D_ROOT_SIGNATURE_VERSION_1_0: VGLog(Rendering) << "Device supports root signature 1.0."; break;
		case D3D_ROOT_SIGNATURE_VERSION_1_1: VGLog(Rendering) << "Device supports root signature 1.1."; break;
		default:
			if (RootSignature.HighestVersion > D3D_ROOT_SIGNATURE_VERSION_1_1)
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

std::pair<std::shared_ptr<Buffer>, size_t> RenderDevice::FrameAllocate(size_t Size)
{
	const auto FrameIndex = Frame % FrameCount;

	FrameBufferOffsets[FrameIndex] += Size;

	return { FrameBuffers[FrameIndex], FrameBufferOffsets[FrameIndex] - Size };
}

DescriptorHandle RenderDevice::AllocateDescriptor(DescriptorType Type)
{
	return DescriptorManager.Allocate(Type);
}

void RenderDevice::SyncInterframe(bool FullSync)
{
	VGScopedCPUStat("Render Sync Interframe");

	// #TEMP
	std::this_thread::sleep_for(200ms);
}

void RenderDevice::SyncIntraframe(SyncType Type)
{
	VGScopedCPUStat("Render Sync Intraframe");

	ID3D12CommandQueue* SyncQueue = nullptr;

	switch (Type)
	{
	case SyncType::Copy:
		SyncQueue = CopyCommandQueue.Get();
		break;
	case SyncType::Direct:
		SyncQueue = DirectCommandQueue.Get();
		break;
	case SyncType::Compute:
		SyncQueue = ComputeCommandQueue.Get();
		break;
	}

	auto Result = SyncQueue->Signal(IntraSyncFence.Get(), ++IntraSyncValue);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to submit signal command to GPU during sync: " << Result;
	}

	if (IntraSyncFence->GetCompletedValue() != IntraSyncValue)
	{
		Result = IntraSyncFence->SetEventOnCompletion(IntraSyncValue, IntraSyncEvent);
		if (FAILED(Result))
		{
			VGLogFatal(Rendering) << "Failed to set fence completion event during sync: " << Result;
		}

		WaitForSingleObject(IntraSyncEvent, INFINITE);
	}
}

void RenderDevice::FrameStep()
{
	VGScopedCPUStat("Frame Step");

	// Make sure we don't get too many CPU frames ahead of the GPU.
	SyncInterframe(false);

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

	SyncInterframe(true);

	RenderWidth = Width;
	RenderHeight = Height;
	Fullscreen = InFullscreen;

	// #TODO: Fullscreen.

	BackBufferTextures = {};  // Release the render targets.

	auto Result = SwapChain->ResizeBuffers(static_cast<UINT>(FrameCount), static_cast<UINT>(Width), static_cast<UINT>(Height), DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to resize swap chain buffers: " << Result;
	}

	SetupRenderTargets();
}