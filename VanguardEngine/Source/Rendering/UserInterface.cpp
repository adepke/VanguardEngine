// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/UserInterface.h>
#include <Rendering/Base.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Rendering/PipelineState.h>
#include <Core/Config.h>
#include <Core/Input.h>
#include <Window/WindowFrame.h>

#include <imgui.h>
#include <d3dcompiler.h>

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

	List.BindPipelineState(*Pipeline);

	D3D12_INDEX_BUFFER_VIEW ibv;
	memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
	ibv.BufferLocation = Resources->IndexBuffer->GetGPUVirtualAddress();
	ibv.SizeInBytes = Resources->IndexBufferSize * sizeof(ImDrawIdx);
	ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	List.Native()->IASetIndexBuffer(&ibv);

	List.Native()->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);
	List.Native()->SetDescriptorHeaps(1, FontHeap.Indirect());

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

		g_pFontTextureResource = {};

		Device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(g_pFontTextureResource.Indirect()));

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
		dstLocation.pResource = g_pFontTextureResource.Get();
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = 0;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = g_pFontTextureResource.Get();
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
		Device->Native()->CreateShaderResourceView(g_pFontTextureResource.Get(), &srvDesc, FontHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// Store our identifier
	io.Fonts->TexID = (ImTextureID)FontHeap->GetGPUDescriptorHandleForHeapStart().ptr;
}

void UserInterfaceManager::CreateDeviceObjects()
{
	if (Pipeline)
		InvalidateDeviceObjects();

	Pipeline = std::make_unique<PipelineState>();

	PipelineStateDescription Description{};
	Description.ShaderPath = Config::ShadersPath / "UserInterface";
	Description.BlendDescription.AlphaToCoverageEnable = false;
	Description.BlendDescription.RenderTarget[0].BlendEnable = true;
	Description.BlendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	Description.BlendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	Description.BlendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	Description.BlendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	Description.BlendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	Description.BlendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	Description.BlendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	Description.RasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	Description.RasterizerDescription.CullMode = D3D12_CULL_MODE_NONE;
	Description.RasterizerDescription.FrontCounterClockwise = false;
	Description.RasterizerDescription.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	Description.RasterizerDescription.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	Description.RasterizerDescription.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	Description.RasterizerDescription.DepthClipEnable = true;
	Description.RasterizerDescription.MultisampleEnable = false;
	Description.RasterizerDescription.AntialiasedLineEnable = false;
	Description.RasterizerDescription.ForcedSampleCount = 0;
	Description.RasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	Description.DepthStencilDescription.DepthEnable = false;
	Description.DepthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	Description.DepthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	Description.DepthStencilDescription.StencilEnable = false;
	Description.DepthStencilDescription.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	Description.DepthStencilDescription.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	Description.DepthStencilDescription.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	Description.DepthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	Description.DepthStencilDescription.BackFace = Description.DepthStencilDescription.FrontFace;
	Description.Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	Pipeline->Build(*Device, Description);

	// Create font heap.
	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.NumDescriptors = 1;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.NodeMask = 0;

	if (Device->Native()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(FontHeap.Indirect())) != S_OK)
		return;

	CreateFontTexture();
}

void UserInterfaceManager::InvalidateDeviceObjects()
{
	g_pVertexShaderBlob = {};
	g_pPixelShaderBlob = {};
	Pipeline.reset();
	g_pFontTextureResource = {};
	FontHeap = {};

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

	auto& Style = ImGui::GetStyle();
	Style.Colors[ImGuiCol_WindowBg].w = 1.f;  // Opaque window backgrounds.

	// Setup back-end capabilities flags
	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererName = "ImGui DirectX 12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;  // #TODO: Navigation features.

	g_pFrameResources = new FrameResources[Device->FrameCount];
	g_numFramesInFlight = Device->FrameCount;
	g_frameIndex = UINT_MAX;

	// Create buffers with a default size (they will later be grown as needed)
	for (uint32_t i = 0; i < g_numFramesInFlight; i++)
	{
		FrameResources* fr = &g_pFrameResources[i];
		fr->IndexBuffer = NULL;
		fr->VertexBuffer = NULL;
		fr->IndexBufferSize = 10000;
		fr->VertexBufferSize = 5000;
	}
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
	if (!Pipeline)
		CreateDeviceObjects();

	auto& io = ImGui::GetIO();
	io.DisplaySize = { static_cast<float>(Device->RenderWidth), static_cast<float>(Device->RenderHeight) };

	// Update inputs.
	Input::UpdateInputDevices(Renderer::Get().Window->GetHandle());

	ImGui::NewFrame();

	// Update the mouse after computing the movement delta.
	Renderer::Get().Window->UpdateCursor();
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
				List.Native()->SetGraphicsRootDescriptorTable(2, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId);
				List.Native()->RSSetScissorRects(1, &r);
				List.Native()->SetGraphicsRootShaderResourceView(1, Resources->VertexBuffer->GetGPUVirtualAddress() + (pcmd->VtxOffset + global_vtx_offset) * sizeof(ImDrawVert));
				List.Native()->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, 0, 0);
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}