// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Viewport.h>
#include <Utility/Singleton.h>

#include <entt/entt.hpp>

class EditorUI : public Singleton<EditorUI>
{
public:
	Viewport SceneViewport;

public:
	void DrawScene();
	void DrawEntityViewer(entt::registry& Registry);
};