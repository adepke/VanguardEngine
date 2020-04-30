// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <Core/Windows/DirectX12Minimal.h>

class RenderDevice;
struct ImDrawData;
class CommandList;
struct FrameResources;

class UserInterfaceManager
{
private:
	RenderDevice* Device;

	ResourcePtr<ID3D10Blob> g_pVertexShaderBlob;
	ResourcePtr<ID3D10Blob> g_pPixelShaderBlob;
	ResourcePtr<ID3D12RootSignature> g_pRootSignature;
	ResourcePtr<ID3D12PipelineState> g_pPipelineState;
	ResourcePtr<ID3D12Resource> g_pFontTextureResource;
	ResourcePtr<ID3D12DescriptorHeap> FontHeap;

	const DXGI_FORMAT g_RTVFormat = DXGI_FORMAT_B8G8R8A8_UNORM;  // #TODO: Centralize RTV format.

private:
	void SetupRenderState(ImDrawData* DrawData, CommandList& List, FrameResources* Resources);
	void CreateFontTexture();
	void CreateDeviceObjects();
	void InvalidateDeviceObjects();

public:
	UserInterfaceManager(RenderDevice* InDevice);
	~UserInterfaceManager();

	void NewFrame();
	void Render(CommandList& List);
};