// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Viewport.h>

#include <entt/entt.hpp>

// Rendering interface for the entire editor, this is the single point of rendering access outside of the Editor module.

namespace EditorRenderer
{
	Viewport GetSceneViewport();

	void Render(entt::registry& Registry);
};