// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Utility/Singleton.h>

#include <entt/entt.hpp>

#include <deque>

class Atmosphere;

class EditorUI : public Singleton<EditorUI>
{
private:
	entt::entity hierarchySelectedEntity = entt::null;
	bool linearizeDepth = true;

	// Window states.
	bool entityHierarchyOpen = true;
	bool entityPropertyViewerOpen = true;
	bool performanceMetricsOpen = true;
	bool renderGraphOpen = true;
	bool atmosphereControlsOpen = true;

	// Focus states.
	bool entityPropertyViewerFocus = false;

	std::deque<float> frameTimes;
	size_t frameTimeHistoryCount = 0;

private:
	void DrawMenu();
	void DrawFrameTimeHistory();

public:
	void DrawLayout();
	void DrawDemoWindow();
	void DrawScene(RenderDevice* device, entt::registry& registry, TextureHandle sceneTexture);
	void DrawEntityHierarchy(entt::registry& registry);
	void DrawEntityPropertyViewer(entt::registry& registry);
	void DrawPerformanceMetrics(float frameTimeMs);
	void DrawRenderGraph(RenderDevice* device, TextureHandle depthStencil, TextureHandle scene);
	void DrawAtmosphereControls(Atmosphere& atmosphere);
};