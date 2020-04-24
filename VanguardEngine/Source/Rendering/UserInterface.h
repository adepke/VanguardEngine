// Copyright (c) 2019-2020 Andrew Depke

#pragma once

class RenderDevice;
struct ImDrawData;
class CommandList;
struct FrameResources;

class UserInterfaceManager
{
private:
	RenderDevice* Device;

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