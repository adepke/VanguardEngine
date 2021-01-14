// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Viewport.h>
#include <Utility/Singleton.h>

#include <entt/entt.hpp>

class EditorUI : public Singleton<EditorUI>
{
private:
	entt::entity hierarchySelectedEntity = entt::null;

public:
	void DrawLayout();
	void DrawScene();
	void DrawEntityHierarchy(entt::registry& registry);
	void DrawEntityPropertyViewer(entt::registry& registry);
};