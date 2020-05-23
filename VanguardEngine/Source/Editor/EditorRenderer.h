// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Viewport.h>

#include <entt/entt.hpp>  // #TODO: Don't include this here.

struct EditorRenderer
{
	static Viewport GetSceneViewport();

	static void Render(entt::registry& Registry);
};