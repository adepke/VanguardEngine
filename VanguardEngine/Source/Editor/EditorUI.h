// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/ResourceHandle.h>

#include <entt/entt.hpp>
#include <imgui.h>

#include <deque>
#include <string>

enum class RenderOverlay
{
	None,
	Clusters,
	HiZ
};

class RenderDevice;
class RenderGraphResourceManager;
class Atmosphere;
class Clouds;
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
	bool controlsOpen = true;
	bool entityHierarchyOpen = true;
	bool entityPropertyViewerOpen = true;
	bool metricsOpen = true;
	bool renderGraphOpen = true;
	bool atmosphereControlsOpen = true;
	bool bloomControlsOpen = true;
	bool renderVisualizerOpen = true;
	bool consoleOpen = false;

	// Focus states.
	bool entityPropertyViewerFocus = false;
	bool consoleClosedThisFrame = false;  // If the console was just closed.
	bool consoleInputFocus = false;

	std::deque<float> frameTimes;
	size_t frameTimeHistoryCount = 0;
	std::deque<std::string> consoleMessages;
	bool needsScrollUpdate = true;
	bool consoleFullyScrolled = false;

	// Scene drawing information.
	float sceneWidthUV;
	float sceneHeightUV;
	ImVec2 sceneViewportMin;
	ImVec2 sceneViewportMax;

	bool renderOverlayOnScene = false;
	float overlayAlpha = 0.5f;

public:
	// Debug/visualization overlay state.
	RenderOverlay activeOverlay = RenderOverlay::None;
	TextureHandle overlayTexture;
	int hiZOverlayMip = 0;

	bool showFps = false;

private:
	void DrawMenu();
	void DrawFrameTimeHistory();
	void DrawRenderOverlayTools(RenderDevice* device, const ImVec2& min, const ImVec2& max);
	void DrawRenderOverlayProxy(RenderDevice* device, const ImVec2& min, const ImVec2& max);
	bool ExecuteCommand(const std::string& command);
	void DrawConsole(entt::registry& registry, const ImVec2& min, const ImVec2& max);

public:
	void Update();
	void DrawLayout();
	void DrawDemoWindow();
	void DrawScene(RenderDevice* device, entt::registry& registry, TextureHandle sceneTexture);
	void DrawControls(RenderDevice* device);
	void DrawEntityHierarchy(entt::registry& registry);
	void DrawEntityPropertyViewer(entt::registry& registry);
	void DrawMetrics(RenderDevice* device, float frameTimeMs);
	void DrawRenderGraph(RenderDevice* device, RenderGraphResourceManager& resourceManager, TextureHandle depthStencil, TextureHandle scene);
	void DrawAtmosphereControls(RenderDevice* device, entt::registry& registry, Atmosphere& atmosphere, Clouds& clouds, TextureHandle weather);
	void DrawBloomControls(Bloom& bloom);
	void DrawRenderVisualizer(RenderDevice* device, ClusteredLightCulling& clusteredCulling, TextureHandle overlay);

	void AddConsoleMessage(const std::string& message);
};