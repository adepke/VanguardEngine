// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/PipelineState.h>
#include <Rendering/ResourceHandle.h>

class RenderDevice;
class CommandList;
struct TextureComponent;

class Mipmapper
{
private:
	PipelineState layout2dState;
	PipelineState layout3dState;

public:
	void Initialize(RenderDevice& device);

	void Generate2D(RenderDevice& device, CommandList& list, TextureHandle texture, TextureComponent& component);
	void Generate3D(RenderDevice& device, CommandList& list, TextureHandle texture, TextureComponent& component);
};