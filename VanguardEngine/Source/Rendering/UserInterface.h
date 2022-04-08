// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderPipeline.h>

#include <Core/Windows/DirectX12Minimal.h>

#include <memory>

class RenderDevice;
struct ImDrawData;
class CommandList;
struct FrameResources;

struct UserInterfaceState
{
	bool linearizeDepth = false;
};

class UserInterfaceManager
{
private:
	bool initialized = false;
	RenderDevice* device;

	ResourcePtr<ID3D10Blob> vertexShaderBlob;
	ResourcePtr<ID3D10Blob> pixelShaderBlob;
	RenderPipelineLayout pipelineLayout;

private:
	XMMATRIX SetupRenderState(ImDrawData* drawData, CommandList& list, FrameResources* resources);
	void CreateFontTexture();
	void CreateDeviceObjects();
	void InvalidateDeviceObjects();

public:
	UserInterfaceManager(RenderDevice* inDevice);
	~UserInterfaceManager();

	void NewFrame();
	void Render(CommandList& list, BufferHandle cameraBuffer);
};