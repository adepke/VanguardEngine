// Copyright (c) 2019-2021 Andrew Depke

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

#include <cstring>

struct FrameResources
{
	ID3D12Resource* indexBuffer;
	ID3D12Resource* vertexBuffer;
	int indexBufferSize;
	int vertexBufferSize;
};

static FrameResources* frameResources = NULL;
static UINT numFramesInFlight = 0;
static UINT frameIndex = UINT_MAX;

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

void UserInterfaceManager::SetupRenderState(ImDrawData* drawData, CommandList& list, FrameResources* resources)
{
	VGScopedCPUStat("Setup Render State");

	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
	VertexConstantBuffer vertexConstantBuffer;
	{
		float L = drawData->DisplayPos.x;
		float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
		float T = drawData->DisplayPos.y;
		float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
		float mvp[4][4] =
		{
			{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
			{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
			{ 0.0f,         0.0f,           0.5f,       0.0f },
			{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
		};
		memcpy(&vertexConstantBuffer.mvp, mvp, sizeof(mvp));
	}

	// Setup viewport
	D3D12_VIEWPORT vp;
	memset(&vp, 0, sizeof(D3D12_VIEWPORT));
	vp.Width = drawData->DisplaySize.x;
	vp.Height = drawData->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	list.Native()->RSSetViewports(1, &vp);

	list.BindPipelineState(*pipeline);

	D3D12_INDEX_BUFFER_VIEW ibv;
	memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
	ibv.BufferLocation = resources->indexBuffer->GetGPUVirtualAddress();
	ibv.SizeInBytes = resources->indexBufferSize * sizeof(ImDrawIdx);
	ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	list.Native()->IASetIndexBuffer(&ibv);

	list.Native()->SetGraphicsRoot32BitConstants(0, 16, &vertexConstantBuffer, 0);
	list.Native()->SetDescriptorHeaps(1, fontHeap.Indirect());

	// Setup blend factor
	const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	list.Native()->OMSetBlendFactor(blendFactor);
}

void UserInterfaceManager::CreateFontTexture()
{
	VGScopedCPUStat("Create Font Texture");

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

		fontTextureResource = {};

		device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(fontTextureResource.Indirect()));

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
		HRESULT hr = device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
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
		dstLocation.pResource = fontTextureResource.Get();
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = 0;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = fontTextureResource.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		ID3D12Fence* fence = NULL;
		hr = device->Native()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		IM_ASSERT(SUCCEEDED(hr));

		HANDLE event = CreateEvent(0, 0, 0, 0);
		IM_ASSERT(event != NULL);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;

		ID3D12CommandQueue* cmdQueue = NULL;
		hr = device->Native()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12CommandAllocator* cmdAlloc = NULL;
		hr = device->Native()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
		IM_ASSERT(SUCCEEDED(hr));

		ID3D12GraphicsCommandList* cmdList = NULL;
		hr = device->Native()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
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
		device->Native()->CreateShaderResourceView(fontTextureResource.Get(), &srvDesc, fontHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// Store our identifier
	io.Fonts->TexID = (ImTextureID)fontHeap->GetGPUDescriptorHandleForHeapStart().ptr;
}

void UserInterfaceManager::CreateDeviceObjects()
{
	VGScopedCPUStat("Create Device Objects");

	if (pipeline)
		InvalidateDeviceObjects();

	pipeline = std::make_unique<PipelineState>();

	PipelineStateDescription description{};
	description.shaderPath = Config::shadersPath / "UserInterface";
	description.blendDescription.AlphaToCoverageEnable = false;
	description.blendDescription.RenderTarget[0].BlendEnable = true;
	description.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	description.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	description.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	description.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	description.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	description.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	description.blendDescription.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	description.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	description.rasterizerDescription.CullMode = D3D12_CULL_MODE_NONE;
	description.rasterizerDescription.FrontCounterClockwise = false;
	description.rasterizerDescription.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	description.rasterizerDescription.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	description.rasterizerDescription.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	description.rasterizerDescription.DepthClipEnable = true;
	description.rasterizerDescription.MultisampleEnable = false;
	description.rasterizerDescription.AntialiasedLineEnable = false;
	description.rasterizerDescription.ForcedSampleCount = 0;
	description.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	description.depthStencilDescription.DepthEnable = false;
	description.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	description.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	description.depthStencilDescription.StencilEnable = false;
	description.depthStencilDescription.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	description.depthStencilDescription.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	description.depthStencilDescription.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	description.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	description.depthStencilDescription.BackFace = description.depthStencilDescription.FrontFace;
	description.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	pipeline->Build(*device, description);

	// Create font heap.
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	if (device->Native()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(fontHeap.Indirect())) != S_OK)
		return;

	CreateFontTexture();
}

void UserInterfaceManager::InvalidateDeviceObjects()
{
	VGScopedCPUStat("Invalidate Device Objects");

	vertexShaderBlob = {};
	pixelShaderBlob = {};
	pipeline.reset();
	fontTextureResource = {};
	fontHeap = {};

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->TexID = NULL; // We copied g_pFontTextureView to io.Fonts->TexID so let's clear that as well.

	for (UINT i = 0; i < numFramesInFlight; i++)
	{
		FrameResources* fr = &frameResources[i];
		SafeRelease(fr->indexBuffer);
		SafeRelease(fr->vertexBuffer);
	}
}

UserInterfaceManager::UserInterfaceManager(RenderDevice* inDevice) : device(inDevice)
{
	VGScopedCPUStat("UI Initialize");

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	auto& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_WindowBg].w = 1.f;  // Opaque window backgrounds.

	const auto configPath = (Config::engineRootPath / "Config/UserInterface.ini").generic_string();

	static char stableIniFilePath[260];  // We need to extend the lifetime of the config path to be available throughout the entire application lifetime.
	strcpy_s(stableIniFilePath, std::size(stableIniFilePath), configPath.c_str());

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = stableIniFilePath;
	io.BackendRendererName = "Vanguard Direct3D 12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;  // #TODO: Navigation features.
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	frameResources = new FrameResources[device->frameCount];
	numFramesInFlight = device->frameCount;
	frameIndex = UINT_MAX;

	// Create buffers with a default size (they will later be grown as needed)
	for (uint32_t i = 0; i < numFramesInFlight; i++)
	{
		FrameResources* fr = &frameResources[i];
		fr->indexBuffer = NULL;
		fr->vertexBuffer = NULL;
		fr->indexBufferSize = 10000;
		fr->vertexBufferSize = 5000;
	}
}

UserInterfaceManager::~UserInterfaceManager()
{
	VGScopedCPUStat("UI Destroy");

	InvalidateDeviceObjects();
	delete[] frameResources;
	frameResources = NULL;
	device = NULL;
	numFramesInFlight = 0;
	frameIndex = UINT_MAX;

	ImGui::DestroyContext();
}

void UserInterfaceManager::NewFrame()
{
	VGScopedCPUStat("UI New Frame");

	if (!pipeline)
		CreateDeviceObjects();

	auto& io = ImGui::GetIO();
	io.DisplaySize = { static_cast<float>(device->renderWidth), static_cast<float>(device->renderHeight) };

	// Update inputs.
	Input::UpdateInputDevices(Renderer::Get().window->GetHandle());

	ImGui::NewFrame();

	// Update the mouse after computing the movement delta.
	Renderer::Get().window->UpdateCursor();
}

void UserInterfaceManager::Render(CommandList& list)
{
	VGScopedCPUStat("UI Render");

	ImGui::Render();
	auto* drawData = ImGui::GetDrawData();

	// Avoid rendering when minimized
	if (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
		return;

	// FIXME: I'm assuming that this only gets called once per frame!
	// If not, we can't just re-allocate the IB or VB, we'll have to do a proper allocator.
	frameIndex = frameIndex + 1;
	FrameResources* resources = &frameResources[frameIndex % numFramesInFlight];

	// Create and grow vertex/index buffers if needed
	if (resources->vertexBuffer == NULL || resources->vertexBufferSize < drawData->TotalVtxCount)
	{
		SafeRelease(resources->vertexBuffer);
		resources->vertexBufferSize = drawData->TotalVtxCount + 5000;
		D3D12_HEAP_PROPERTIES props;
		memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(D3D12_RESOURCE_DESC));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = resources->vertexBufferSize * sizeof(ImDrawVert);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&resources->vertexBuffer)) < 0)
			return;
	}
	if (resources->indexBuffer == NULL || resources->indexBufferSize < drawData->TotalIdxCount)
	{
		SafeRelease(resources->indexBuffer);
		resources->indexBufferSize = drawData->TotalIdxCount + 10000;
		D3D12_HEAP_PROPERTIES props;
		memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
		props.Type = D3D12_HEAP_TYPE_UPLOAD;
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(D3D12_RESOURCE_DESC));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = resources->indexBufferSize * sizeof(ImDrawIdx);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (device->Native()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&resources->indexBuffer)) < 0)
			return;
	}

	// Upload vertex/index data into a single contiguous GPU buffer
	void* vtx_resource, * idx_resource;
	D3D12_RANGE range;
	memset(&range, 0, sizeof(D3D12_RANGE));
	if (resources->vertexBuffer->Map(0, &range, &vtx_resource) != S_OK)
		return;
	if (resources->indexBuffer->Map(0, &range, &idx_resource) != S_OK)
		return;
	ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
	ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = drawData->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	resources->vertexBuffer->Unmap(0, &range);
	resources->indexBuffer->Unmap(0, &range);

	// Setup desired DX state
	SetupRenderState(drawData, list, resources);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_vtx_offset = 0;
	int global_idx_offset = 0;
	ImVec2 clip_off = drawData->DisplayPos;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = drawData->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					SetupRenderState(drawData, list, resources);
				else
					pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Apply Scissor, Bind texture, Draw
				const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
				list.Native()->SetGraphicsRootDescriptorTable(2, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId);
				list.Native()->RSSetScissorRects(1, &r);
				list.Native()->SetGraphicsRootShaderResourceView(1, resources->vertexBuffer->GetGPUVirtualAddress() + (pcmd->VtxOffset + global_vtx_offset) * sizeof(ImDrawVert));
				list.Native()->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, 0, 0);
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}