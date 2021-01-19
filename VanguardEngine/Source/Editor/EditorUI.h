// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/Viewport.h>
#include <Utility/Singleton.h>

#include <entt/entt.hpp>

class EditorUI : public Singleton<EditorUI>
{
private:
	entt::entity hierarchySelectedEntity = entt::null;

	// Window states.
	bool entityHierarchyOpen = true;
	bool entityPropertyViewerOpen = true;
	bool renderGraphOpen = false;

private:
	void DrawMenu();

public:
	void DrawLayout();
	void DrawDemoWindow();
	void DrawScene(RenderDevice* device, TextureHandle sceneTexture);
	void DrawEntityHierarchy(entt::registry& registry);
	void DrawEntityPropertyViewer(entt::registry& registry);
	void DrawRenderGraph(RenderDevice* device, TextureHandle depthStencil, TextureHandle scene);
};