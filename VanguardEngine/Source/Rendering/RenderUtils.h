// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Utility/Singleton.h>
#include <Rendering/PipelineState.h>
#include <Rendering/ResourceHandle.h>

class RenderDevice;
class CommandList;

class RenderUtils : public Singleton<RenderUtils>
{
private:
	RenderDevice* device = nullptr;
	PipelineState clearUAVState;

public:
	void Initialize(RenderDevice* inDevice);

	void ClearUAV(CommandList& list, BufferHandle buffer);
};