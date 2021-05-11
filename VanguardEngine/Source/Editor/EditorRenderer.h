// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/Viewport.h>

#include <entt/entt.hpp>

class RenderDevice;
class Atmosphere;

// Rendering interface for the entire editor, this is the single point of rendering access outside of the Editor module.

namespace EditorRenderer
{
	void Render(RenderDevice* device, entt::registry& registry, float frameTimeMs, TextureHandle depthStencilTexture, TextureHandle sceneTexture, Atmosphere& atmosphere);
};