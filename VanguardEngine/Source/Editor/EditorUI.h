// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/ResourceHandle.h>

#include <entt/entt.hpp>
#include <imgui.h>

#include <deque>

enum class RenderOverlay
{
	None,
	Clusters
};

class RenderDevice;
class RenderGraphResourceManager;
class Atmosphere;
class Bloom;
class ClusteredLightCulling;

class EditorUI
{
private:
	bool enabled = true;

	entt::entity hierarchySelectedEntity = entt::null;
	bool linearizeDepth = true;

	bool fullscreen = false;

	// Window states.
	bool entityHierarchyOpen = true;
	bool entityPropertyViewerOpen = true;
	bool metricsOpen = true;
	bool renderGraphOpen = true;
	bool atmosphereControlsOpen = true;
	bool bloomControlsOpen = true;
	bool renderVisualizerOpen = true;

	// Focus states.
	bool entityPropertyViewerFocus = false;

	std::deque<float> frameTimes;
	size_t frameTimeHistoryCount = 0;

	// Scene drawing information.
	float sceneWidthUV;
	float sceneHeightUV;
	ImVec2 sceneViewportMin;
	ImVec2 sceneViewportMax;

	bool renderOverlayOnScene = false;

public:
	// Debug/visualization overlay state.
	RenderOverlay activeOverlay = RenderOverlay::None;

private:
	void DrawMenu();
	void DrawFrameTimeHistory();

public:
	void Update();
	void DrawLayout();
	void DrawDemoWindow();
	void DrawScene(RenderDevice* device, entt::registry& registry, TextureHandle sceneTexture);
	void DrawEntityHierarchy(entt::registry& registry);
	void DrawEntityPropertyViewer(entt::registry& registry);
	void DrawMetrics(RenderDevice* device, float frameTimeMs);
	void DrawRenderGraph(RenderDevice* device, RenderGraphResourceManager& resourceManager, TextureHandle depthStencil, TextureHandle scene);
	void DrawAtmosphereControls(Atmosphere& atmosphere);
	void DrawBloomControls(Bloom& bloom);
	void DrawRenderVisualizer(RenderDevice* device, ClusteredLightCulling& clusteredCulling, TextureHandle overlay);
};