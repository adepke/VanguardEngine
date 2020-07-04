// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Viewport.h>
#include <Utility/Singleton.h>

#include <entt/entt.hpp>

class EditorUI : public Singleton<EditorUI>
{
private:
	entt::entity hierarchySelectedEntity = entt::null;

public:
	Viewport sceneViewport;

public:
	void DrawScene();
	void DrawToolbar();
	void DrawEntityHierarchy(entt::registry& registry);
	void DrawEntityPropertyViewer(entt::registry& registry);
	void DrawAssetBrowser();
};