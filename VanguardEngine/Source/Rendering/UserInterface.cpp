// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/UserInterface.h>
#include <Rendering/Base.h>
#include <Rendering/Device.h>
#include <Rendering/Resource.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/Renderer.h>
#include <Core/Config.h>
#include <Core/Input.h>
#include <Window/WindowFrame.h>
#include <Utility/AlignedSize.h>
#include <Editor/ImGuiExtensions.h>

#include <imgui.h>
#include <d3dcompiler.h>

#include <cstring>

struct FrameResources
{
	BufferHandle indexBuffer;
	BufferHandle vertexBuffer;
	size_t indexBufferSize;
	size_t vertexBufferSize;
};

static FrameResources* frameResources = NULL;
static UINT numFramesInFlight = 0;
static UINT frameIndex = UINT_MAX;

XMMATRIX UserInterfaceManager::SetupRenderState(ImDrawData* drawData, CommandList& list, FrameResources* resources)
{
	VGScopedCPUStat("Setup Render State");

	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
	const float L = drawData->DisplayPos.x;
	const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
	const float T = drawData->DisplayPos.y;
	const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
	const float mvp[4][4] =
	{
		{ 2.0f / (R - L),    0.0f,              0.0f, 0.0f },
		{ 0.0f,              2.0f / (T - B),    0.0f, 0.0f },
		{ 0.0f,              0.0f,              0.5f, 0.0f },
		{ (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
	};

	// Setup viewport
	D3D12_VIEWPORT vp;
	memset(&vp, 0, sizeof(D3D12_VIEWPORT));
	vp.Width = drawData->DisplaySize.x;
	vp.Height = drawData->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	list.Native()->RSSetViewports(1, &vp);

	list.BindPipeline(pipelineLayout);

	auto& indexBuffer = device->GetResourceManager().Get(resources->indexBuffer);

	D3D12_INDEX_BUFFER_VIEW ibv;
	memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
	ibv.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress();
	ibv.SizeInBytes = resources->indexBufferSize * sizeof(ImDrawIdx);
	ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	list.Native()->IASetIndexBuffer(&ibv);

	list.BindDescriptorAllocator(device->GetDescriptorAllocator());

	// Setup blend factor
	const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	list.Native()->OMSetBlendFactor(blendFactor);

	return XMMATRIX{ (const float*)mvp };
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

	TextureDescription fontDesc{
		.bindFlags = BindFlag::ShaderResource,
		.accessFlags = AccessFlag::CPUWrite,
		.width = (uint32_t)width,
		.height = (uint32_t)height,
		.depth = 1,
		.format = DXGI_FORMAT_R8G8B8A8_UNORM  // Fonts can be either linear or sRGB, full white maps to the same value in each color space. Use linear here to avoid unnecessary hardware conversion.
	};

	const auto fontHandle = device->GetResourceManager().Create(fontDesc, VGText("ImGui font texture"));

	UINT uploadPitch = AlignedSize(width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	UINT uploadSize = height * uploadPitch;

	std::vector<uint8_t> textureData;
	textureData.resize(uploadSize);

	for (int y = 0; y < height; y++)
	{
		memcpy(textureData.data() + (y * uploadPitch), pixels + y * width * 4, width * 4);
	}

	device->GetResourceManager().Write(fontHandle, textureData);
	device->GetDirectList().TransitionBarrier(fontHandle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	io.Fonts->TexID = (ImTextureID)device->GetResourceManager().Get(fontHandle).SRV->bindlessIndex;
}

void UserInterfaceManager::CreateDeviceObjects()
{
	VGScopedCPUStat("Create Device Objects");

	if (initialized)
		InvalidateDeviceObjects();

	pipelineLayout = RenderPipelineLayout{}
		.VertexShader({ "UserInterface", "VSMain" })
		.PixelShader({ "UserInterface", "PSMain" })
		.BlendMode(true, {
			.srcBlend = D3D12_BLEND_SRC_ALPHA,
			.destBlend = D3D12_BLEND_INV_SRC_ALPHA,
			.blendOp = D3D12_BLEND_OP_ADD,
			.srcBlendAlpha = D3D12_BLEND_ONE,
			.destBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
			.blendOpAlpha = D3D12_BLEND_OP_ADD
		})
		.CullMode(D3D12_CULL_MODE_NONE)
		.DepthEnabled(false);

	CreateFontTexture();

	initialized = true;
}

void UserInterfaceManager::InvalidateDeviceObjects()
{
	VGScopedCPUStat("Invalidate Device Objects");

	initialized = false;
	vertexShaderBlob = {};
	pixelShaderBlob = {};

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->TexID = NULL; // We copied g_pFontTextureView to io.Fonts->TexID so let's clear that as well.

	for (UINT i = 0; i < numFramesInFlight; i++)
	{
		FrameResources* fr = &frameResources[i];
		if (fr->indexBuffer.handle != entt::null)
			device->GetResourceManager().Destroy(fr->indexBuffer);
		if (fr->vertexBuffer.handle != entt::null)
			device->GetResourceManager().Destroy(fr->vertexBuffer);
	}
}

UserInterfaceManager::UserInterfaceManager(RenderDevice* inDevice) : device(inDevice)
{
	VGScopedCPUStat("UI Initialize");

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsVanguard();

	const auto configPath = (Config::engineRootPath / "Config/UserInterface.ini").generic_string();

	static char stableIniFilePath[260];  // We need to extend the lifetime of the config path to be available throughout the entire application lifetime.
	strcpy_s(stableIniFilePath, std::size(stableIniFilePath), configPath.c_str());

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = stableIniFilePath;
	io.BackendRendererName = "Vanguard Direct3D 12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;  // #TODO: Navigation features.
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	// Load the font.
	// #TODO: Improved font handling.
	const auto fontPath = Config::fontsPath / "Cousine-Regular.ttf";
	if (!io.Fonts->AddFontFromFileTTF(fontPath.generic_string().c_str(), 15.f, nullptr, nullptr))
	{
		VGLogWarning(logCore, "Failed to load custom font, falling back to default font.");
	}

	frameResources = new FrameResources[device->frameCount];
	numFramesInFlight = device->frameCount;
	frameIndex = UINT_MAX;

	// Create buffers with a default size (they will later be grown as needed)
	for (uint32_t i = 0; i < numFramesInFlight; i++)
	{
		FrameResources* fr = &frameResources[i];
		fr->indexBuffer.handle = entt::null;
		fr->vertexBuffer.handle = entt::null;
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

	if (!initialized)
		CreateDeviceObjects();

	auto& io = ImGui::GetIO();
	io.DisplaySize = { static_cast<float>(device->renderWidth), static_cast<float>(device->renderHeight) };

	// Update inputs.
	Input::UpdateInputDevices(Renderer::Get().window->GetHandle());

	// Update the mouse before computing the movement delta in NewFrame().
	Renderer::Get().window->UpdateCursor();

	ImGui::NewFrame();
}

void UserInterfaceManager::Render(CommandList& list, BufferHandle cameraBuffer)
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
	if (resources->vertexBuffer.handle == entt::null || resources->vertexBufferSize < drawData->TotalVtxCount)
	{
		if (resources->vertexBuffer.handle != entt::null)
			device->GetResourceManager().Destroy(resources->vertexBuffer);
		resources->vertexBufferSize = drawData->TotalVtxCount + 5000;

		BufferDescription vertexBufferDesc{
			.updateRate = ResourceFrequency::Dynamic,
			.bindFlags = BindFlag::ShaderResource,
			.accessFlags = AccessFlag::CPUWrite,
			.size = resources->vertexBufferSize,
			.stride = sizeof(ImDrawVert)
		};

		resources->vertexBuffer = device->GetResourceManager().Create(vertexBufferDesc, VGText("UI vertex buffer"));
	}
	if (resources->indexBuffer.handle == entt::null || resources->indexBufferSize < drawData->TotalIdxCount)
	{
		if (resources->indexBuffer.handle != entt::null)
			device->GetResourceManager().Destroy(resources->indexBuffer);
		resources->indexBufferSize = drawData->TotalIdxCount + 10000;

		BufferDescription indexBufferDesc{
			.updateRate = ResourceFrequency::Dynamic,
			.bindFlags = BindFlag::IndexBuffer,
			.accessFlags = AccessFlag::CPUWrite,
			.size = resources->indexBufferSize,
			.stride = sizeof(ImDrawIdx)
		};

		resources->indexBuffer = device->GetResourceManager().Create(indexBufferDesc, VGText("UI index buffer"));
	}

	// Upload vertex/index data into a single contiguous GPU buffer
	std::vector<uint8_t> vertexData;
	std::vector<uint8_t> indexData;
	size_t vertexOffset = 0;
	size_t indexOffset = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = drawData->CmdLists[n];
		vertexData.resize(vertexData.size() + cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		indexData.resize(indexData.size() + cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		memcpy(vertexData.data() + vertexOffset, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(indexData.data() + indexOffset, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vertexOffset += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
		indexOffset += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
	}

	device->GetResourceManager().Write(resources->vertexBuffer, vertexData);
	device->GetResourceManager().Write(resources->indexBuffer, indexData);

	// Setup desired DX state
	XMMATRIX projectionMatrix = SetupRenderState(drawData, list, resources);
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	UserInterfaceState drawState{};  // Reset each frame.

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
					pcmd->UserCallback(&list, drawState);
			}
			else
			{
				const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
				list.Native()->RSSetScissorRects(1, &r);

				struct Data
				{
					XMMATRIX projectionMatrix;
					uint32_t cameraBuffer;
					uint32_t vertexBuffer;
					uint32_t vertexOffset;
					uint32_t texture;
					uint32_t depthLinearization;
				} data;

				data.projectionMatrix = projectionMatrix;
				data.cameraBuffer = device->GetResourceManager().Get(cameraBuffer).SRV->bindlessIndex;
				data.vertexBuffer = device->GetResourceManager().Get(resources->vertexBuffer).SRV->bindlessIndex;
				data.vertexOffset = pcmd->VtxOffset + global_vtx_offset;
				data.texture = *(uint32_t*)&pcmd->TextureId;
				data.depthLinearization = drawState.linearizeDepth;
				list.BindConstants("data", data);

				list.Native()->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, 0, 0);
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}