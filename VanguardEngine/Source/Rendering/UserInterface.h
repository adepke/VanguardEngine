// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <Core/Windows/DirectX12Minimal.h>

#include <memory>

class RenderDevice;
struct ImDrawData;
class CommandList;
struct FrameResources;
class PipelineState;

class UserInterfaceManager
{
private:
	RenderDevice* device;

	ResourcePtr<ID3D10Blob> vertexShaderBlob;
	ResourcePtr<ID3D10Blob> pixelShaderBlob;
	std::unique_ptr<PipelineState> pipeline;

	const DXGI_FORMAT rtvFormat = DXGI_FORMAT_B8G8R8A8_UNORM;  // #TODO: Centralize RTV format.

private:
	void SetupRenderState(ImDrawData* drawData, CommandList& list, FrameResources* resources);
	void CreateFontTexture();
	void CreateDeviceObjects();
	void InvalidateDeviceObjects();

public:
	UserInterfaceManager(RenderDevice* inDevice);
	~UserInterfaceManager();

	void NewFrame();
	void Render(CommandList& list);
};