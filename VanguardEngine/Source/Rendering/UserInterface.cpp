// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/UserInterface.h>
#include <Rendering/Base.h>
#include <Rendering/Device.h>

#include <Core/Windows/DirectX12Minimal.h>
#include <imgui.h>
#include <d3dcompiler.h>

static ID3D10Blob* g_pVertexShaderBlob = NULL;
static ID3D10Blob* g_pPixelShaderBlob = NULL;
static ID3D12RootSignature* g_pRootSignature = NULL;
static ID3D12PipelineState* g_pPipelineState = NULL;
static DXGI_FORMAT g_RTVFormat = DXGI_FORMAT_UNKNOWN;
static ID3D12Resource* g_pFontTextureResource = NULL;
static ID3D12DescriptorHeap* FontHeap;

struct FrameResources
{
	ID3D12Resource* IndexBuffer;
	ID3D12Resource* VertexBuffer;
	int IndexBufferSize;
	int VertexBufferSize;
};

static FrameResources* g_pFrameResources = NULL;
static UINT g_numFramesInFlight = 0;
static UINT g_frameIndex = UINT_MAX;

template<typename T>
static void SafeRelease(T*& res)
{
	if (res)
		res->Release();
	res = NULL;
}

struct VertexConstantBuffer
{
	float mvp[4][4];
};

void UserInterfaceManager::SetupRenderState(ImDrawData* DrawData, CommandList& List, FrameResources* Resources)
{
	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
	VertexConstantBuffer vertex_constant_buffer;
	{
		float L = DrawData->DisplayPos.x;
		float R = DrawData->DisplayPos.x + DrawData->DisplaySize.x;
		float T = DrawData->DisplayPos.y;
		float B = DrawData->DisplayPos.y + DrawData->DisplaySize.y;
		float mvp[4][4] =
		{
			{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
			{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
			{ 0.0f,         0.0f,           0.5f,       0.0f },
			{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
		};
		memcpy(&vertex_constant_buffer.mvp, mvp, sizeof(mvp));
	}

	// Setup viewport
	D3D12_VIEWPORT vp;
	memset(&vp, 0, sizeof(D3D12_VIEWPORT));
	vp.Width = DrawData->DisplaySize.x;
	vp.Height = DrawData->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	List.Native()->RSSetViewports(1, &vp);

	// Bind shader and vertex buffers
	unsigned int stride = sizeof(ImDrawVert);
	unsigned int offset = 0;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	memset(&vbv, 0, sizeof(D3D12_VERTEX_BUFFER_VIEW));
	vbv.BufferLocation = Resources->VertexBuffer->GetGPUVirtualAddress() + offset;
	vbv.SizeInBytes = Resources->VertexBufferSize * stride;
	vbv.StrideInBytes = stride;
	List.Native()->IASetVertexBuffers(0, 1, &vbv);
	D3D12_INDEX_BUFFER_VIEW ibv;
	memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
	ibv.BufferLocation = Resources->IndexBuffer->GetGPUVirtualAddress();
	ibv.SizeInBytes = Resources->IndexBufferSize * sizeof(ImDrawIdx);
	ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	List.Native()->IASetIndexBuffer(&ibv);
	List.Native()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	List.Native()->SetPipelineState(g_pPipelineState);
	List.Native()->SetGraphicsRootSignature(g_pRootSignature);
	List.Native()->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);
	List.Native()->SetDescriptorHeaps(1, &FontHeap);

	// Setup blend factor
	const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
	List.Native()->OMSetBlendFactor(blend_factor);
}

void UserInterfaceManager::CreateFontTexture()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	// Upload texture to graphics system
	{
		D3D12_HEAP_PROPERTIES props;
		memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
		props.Type = D3D12_HEAP_TYPE_DEFAULT;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = 0;
		desc.Width = width;
		desc.Height = height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* pTexture = NULL;
		Device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture));

		UINT uploadPitch = (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
		UINT uploadSize = height * uploadPitch;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = uploadSize;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		ID3D12Resource* uploadBuffer = NULL;
		HRESULT hr = Device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));
		IM_ASSERT(SUCCEEDED(hr));

		void* mapped = NULL;
		D3D12_RANGE range = { 0, uploadSize };
		hr = uploadBuffer->Map(0, &range, &mapped);
		IM_ASSERT(SUCCEEDED(hr));
		for (int y = 0; y < height; y++)
			memcpy((void*)((uintptr_t)mapped + y * uploadPitch), pixels + y * width * 4, width * 4);
		uploadBuffer->Unmap(0, &range);

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = uploadBuffer;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srcLocation.PlacedFootprint.Footprint.Width = width;
		srcLocation.PlacedFootprint.Footprint.Height = height;
		srcLocation.PlacedFootprint.Footprint.Depth = 1;
		srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = pTexture;
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = 0;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pTexture;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		ID3D12Fence* fence = NULL;
		hr = Device->Native()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		IM_ASSERT(SUCCEEDED(hr));

		HANDLE event = CreateEvent(0, 0, 0, 0);
		IM_ASSERT(event != NULL);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;

		ID3D12CommandQueue* cmdQueue = NULL;
		hr = Device->Native()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12CommandAllocator* cmdAlloc = NULL;
		hr = Device->Native()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12GraphicsCommandList* cmdList = NULL;
		hr = Device->Native()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
		IM_ASSERT(SUCCEEDED(hr));

		cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
		cmdList->ResourceBarrier(1, &barrier);

		hr = cmdList->Close();
		IM_ASSERT(SUCCEEDED(hr));

		cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmdList);
		hr = cmdQueue->Signal(fence, 1);
		IM_ASSERT(SUCCEEDED(hr));

		fence->SetEventOnCompletion(1, event);
		WaitForSingleObject(event, INFINITE);

		cmdList->Release();
		cmdAlloc->Release();
		cmdQueue->Release();
		CloseHandle(event);
		fence->Release();
		uploadBuffer->Release();

		// Create texture view
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Device->Native()->CreateShaderResourceView(pTexture, &srvDesc, FontHeap->GetCPUDescriptorHandleForHeapStart());
		SafeRelease(g_pFontTextureResource);
		g_pFontTextureResource = pTexture;
	}

	// Store our identifier
	io.Fonts->TexID = (ImTextureID)FontHeap->GetGPUDescriptorHandleForHeapStart().ptr;
}

void UserInterfaceManager::CreateDeviceObjects()
{
	if (g_pPipelineState)
		InvalidateDeviceObjects();

	// Create the root signature
	{
		D3D12_DESCRIPTOR_RANGE descRange = {};
		descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descRange.NumDescriptors = 1;
		descRange.BaseShaderRegister = 0;
		descRange.RegisterSpace = 0;
		descRange.OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER param[2] = {};

		param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		param[0].Constants.ShaderRegister = 0;
		param[0].Constants.RegisterSpace = 0;
		param[0].Constants.Num32BitValues = 16;
		param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param[1].DescriptorTable.NumDescriptorRanges = 1;
		param[1].DescriptorTable.pDescriptorRanges = &descRange;
		param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.MipLODBias = 0.f;
		staticSampler.MaxAnisotropy = 0;
		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		staticSampler.MinLOD = 0.f;
		staticSampler.MaxLOD = 0.f;
		staticSampler.ShaderRegister = 0;
		staticSampler.RegisterSpace = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = _countof(param);
		desc.pParameters = param;
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = &staticSampler;
		desc.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		ID3DBlob* blob = NULL;
		if (D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL) != S_OK)
			return;

		Device->Native()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&g_pRootSignature));
		blob->Release();
	}

	// By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
	// If you would like to use this DX12 sample code but remove this dependency you can:
	//  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
	//  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
	// See https://github.com/ocornut/imgui/pull/638 for sources and details.

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	memset(&psoDesc, 0, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.NodeMask = 1;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.pRootSignature = g_pRootSignature;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = g_RTVFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// Create the vertex shader
	{
		static const char* vertexShader =
			"cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float2 pos : POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
              output.col = input.col;\
              output.uv  = input.uv;\
              return output;\
            }";

		D3DCompile(vertexShader, strlen(vertexShader), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &g_pVertexShaderBlob, NULL);
		if (g_pVertexShaderBlob == NULL) // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
			return;
		psoDesc.VS = { g_pVertexShaderBlob->GetBufferPointer(), g_pVertexShaderBlob->GetBufferSize() };

		// Create the input layout
		static D3D12_INPUT_ELEMENT_DESC local_layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		psoDesc.InputLayout = { local_layout, 3 };
	}

	// Create the pixel shader
	{
		static const char* pixelShader =
			"struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            SamplerState sampler0 : register(s0);\
            Texture2D texture0 : register(t0);\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
              float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
              return out_col; \
            }";

		D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &g_pPixelShaderBlob, NULL);
		if (g_pPixelShaderBlob == NULL)  // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
			return;
		psoDesc.PS = { g_pPixelShaderBlob->GetBufferPointer(), g_pPixelShaderBlob->GetBufferSize() };
	}

	// Create the blending setup
	{
		D3D12_BLEND_DESC& desc = psoDesc.BlendState;
		desc.AlphaToCoverageEnable = false;
		desc.RenderTarget[0].BlendEnable = true;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	// Create the rasterizer state
	{
		D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.DepthClipEnable = true;
		desc.MultisampleEnable = FALSE;
		desc.AntialiasedLineEnable = FALSE;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}

	// Create depth-stencil State
	{
		D3D12_DEPTH_STENCIL_DESC& desc = psoDesc.DepthStencilState;
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.StencilEnable = false;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.BackFace = desc.FrontFace;
	}

	if (Device->Native()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_pPipelineState)) != S_OK)
		return;

	// Create font heap.
	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.NumDescriptors = 1;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.NodeMask = 0;

	if (Device->Native()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&FontHeap)) != S_OK)
		return;

	CreateFontTexture();
}

void UserInterfaceManager::InvalidateDeviceObjects()
{
	SafeRelease(g_pVertexShaderBlob);
	SafeRelease(g_pPixelShaderBlob);
	SafeRelease(g_pRootSignature);
	SafeRelease(g_pPipelineState);
	SafeRelease(g_pFontTextureResource);
	SafeRelease(FontHeap);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->TexID = NULL; // We copied g_pFontTextureView to io.Fonts->TexID so let's clear that as well.

	for (UINT i = 0; i < g_numFramesInFlight; i++)
	{
		FrameResources* fr = &g_pFrameResources[i];
		SafeRelease(fr->IndexBuffer);
		SafeRelease(fr->VertexBuffer);
	}
}

UserInterfaceManager::UserInterfaceManager(RenderDevice* InDevice) : Device(InDevice)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Setup back-end capabilities flags
	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererName = "imgui_impl_dx12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

	g_RTVFormat = DXGI_FORMAT_B8G8R8A8_UNORM;  // #TODO: Centralize RTV format.
	g_pFrameResources = new FrameResources[Device->FrameCount];
	g_numFramesInFlight = Device->FrameCount;
	g_frameIndex = UINT_MAX;

	// Create buffers with a default size (they will later be grown as needed)
	for (int i = 0; i < g_numFramesInFlight; i++)
	{
		FrameResources* fr = &g_pFrameResources[i];
		fr->IndexBuffer = NULL;
		fr->VertexBuffer = NULL;
		fr->IndexBufferSize = 10000;
		fr->VertexBufferSize = 5000;
	}

	io.ImeWindowHandle = Device->GetWindowHandle();
}

UserInterfaceManager::~UserInterfaceManager()
{
	InvalidateDeviceObjects();
	delete[] g_pFrameResources;
	g_pFrameResources = NULL;
	Device = NULL;
	g_numFramesInFlight = 0;
	g_frameIndex = UINT_MAX;

	ImGui::DestroyContext();
}

void UserInterfaceManager::NewFrame()
{
	if (!g_pPipelineState)
		CreateDeviceObjects();

	auto& io = ImGui::GetIO();
	io.DisplaySize = { static_cast<float>(Device->RenderWidth), static_cast<float>(Device->RenderHeight) };

	ImGui::NewFrame();
}

void UserInterfaceManager::Render(CommandList& List)
{
	ImGui::Render();
	auto* DrawData = ImGui::GetDrawData();

	// Avoid rendering when minimized
	if (DrawData->DisplaySize.x <= 0.0f || DrawData->DisplaySize.y <= 0.0f)
		return;

	// FIXME: I'm assuming that this only gets called once per frame!
	// If not, we can't just re-allocate the IB or VB, we'll have to do a proper allocator.
	g_frameIndex = g_frameIndex + 1;
	FrameResources* Resources = &g_pFrameResources[g_frameIndex % g_numFramesInFlight];

	// Create and grow vertex/index buffers if needed
	if (Resources->VertexBuffer == NULL || Resources->VertexBufferSize < DrawData->TotalVtxCount)
	{
		SafeRelease(Resources->VertexBuffer);
		Resources->VertexBufferSize = DrawData->TotalVtxCount + 5000;
		D3D12_HEAP_PROPERTIES props;
		memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(D3D12_RESOURCE_DESC));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = Resources->VertexBufferSize * sizeof(ImDrawVert);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (Device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&Resources->VertexBuffer)) < 0)
			return;
	}
	if (Resources->IndexBuffer == NULL || Resources->IndexBufferSize < DrawData->TotalIdxCount)
	{
		SafeRelease(Resources->IndexBuffer);
		Resources->IndexBufferSize = DrawData->TotalIdxCount + 10000;
		D3D12_HEAP_PROPERTIES props;
		memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(D3D12_RESOURCE_DESC));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = Resources->IndexBufferSize * sizeof(ImDrawIdx);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (Device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&Resources->IndexBuffer)) < 0)
			return;
	}

	// Upload vertex/index data into a single contiguous GPU buffer
	void* vtx_resource, * idx_resource;
	D3D12_RANGE range;
	memset(&range, 0, sizeof(D3D12_RANGE));
	if (Resources->VertexBuffer->Map(0, &range, &vtx_resource) != S_OK)
		return;
	if (Resources->IndexBuffer->Map(0, &range, &idx_resource) != S_OK)
		return;
	ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
	ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource;
	for (int n = 0; n < DrawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = DrawData->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	Resources->VertexBuffer->Unmap(0, &range);
	Resources->IndexBuffer->Unmap(0, &range);

	// Setup desired DX state
	SetupRenderState(DrawData, List, Resources);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_vtx_offset = 0;
	int global_idx_offset = 0;
	ImVec2 clip_off = DrawData->DisplayPos;
	for (int n = 0; n < DrawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = DrawData->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					SetupRenderState(DrawData, List, Resources);
				else
					pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Apply Scissor, Bind texture, Draw
				const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
				List.Native()->SetGraphicsRootDescriptorTable(1, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId);
				List.Native()->RSSetScissorRects(1, &r);
				List.Native()->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}