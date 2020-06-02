// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Viewport.h>
#include <Utility/Singleton.h>

#include <entt/entt.hpp>

class EditorUI : public Singleton<EditorUI>
{
private:
	entt::entity HierarchySelectedEntity = entt::null;

public:
	Viewport SceneViewport;

public:
	void DrawScene();
	void DrawToolbar();
	void DrawEntityHierarchy(entt::registry& Registry);
	void DrawEntityPropertyViewer(entt::registry& Registry);
	void DrawAssetBrowser();
};