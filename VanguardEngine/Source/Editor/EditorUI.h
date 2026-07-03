// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/ResourceHandle.h>
#include <Rendering/TextureCapture.h>
#include <Scene/SceneManager.h>

#include <entt/entt.hpp>
#include <imgui.h>

#include <deque>
#include <string>
#include <filesystem>
#include <optional>

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
class CommandList;

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

	// Gizmo state. Stored as ints to avoid pulling ImGuizmo into this header.
	int gizmoOperation = 7;  // TRANSLATE_X | TRANSLATE_Y | TRANSLATE_Z
	int gizmoMode = 1;  // WORLD

	// Smoothed opacity used to fade the floating scene toolbar based on cursor proximity.
	// Held across frames so the fade animates rather than snapping.
	float sceneToolbarOpacity = 0.25f;
	// Screen-space rect of the floating toolbar, populated each frame by DrawSceneToolbar so
	// the picking code can suppress entity picks when a click lands on a toolbar button.
	ImVec2 sceneToolbarMin = { 0.f, 0.f };
	ImVec2 sceneToolbarMax = { 0.f, 0.f };

	// For now, only load scenes once on startup. #TODO: instead of a static list, listen for
	// file changes in the Scenes folder, and reload on change.
	bool refreshScenes = true;
	std::vector<SceneMetadata> loadedScenes;

	// The thumbnail saving is pretty hacky and not extensible, instead the render graph should
	// probably handle this seamlessly. For instance, a pass could request a texture resource
	// as CPU readback and all the sync + state management is handled by the graph.
	std::optional<std::filesystem::path> pendingSavePath;
	TextureCapture::PendingReadback pendingSaveReadback;
	bool pendingSaveCaptureEnqueued = false;

	// Scene context menu state.
	std::optional<std::filesystem::path> pendingDeletePath;
	std::optional<std::filesystem::path> renamingScenePath;
	char renameBuffer[256] = { 0 };

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
	void DrawSelectionGizmo(entt::registry& registry);
	void DrawSceneToolbar(const ImVec2& viewportMin, const ImVec2& viewportMax);
	void DrawSceneIcon(RenderDevice* device, entt::registry& registry, const SceneMetadata& scene);

	// Scene handling
	void FlushPendingSave(RenderDevice& device, entt::registry& registry);
	void RefreshScenes(RenderDevice& device);
	std::filesystem::path PickNextNewScenePath() const;

public:
	void Update(RenderDevice& device, entt::registry& registry);
	void DrawLayout();
	void DrawDemoWindow();
	void DrawScene(RenderDevice* device, entt::registry& registry, TextureHandle sceneTexture);
	void DrawSceneSelector(RenderDevice* device, entt::registry& registry);
	void DrawControls(RenderDevice* device);
	void DrawEntityHierarchy(entt::registry& registry);
	void DrawEntityPropertyViewer(entt::registry& registry);
	void DrawMetrics(RenderDevice* device, float frameTimeMs);
	void DrawRenderGraph(RenderDevice* device, RenderGraphResourceManager& resourceManager, TextureHandle depthStencil, TextureHandle scene);
	void DrawAtmosphereControls(RenderDevice* device, entt::registry& registry, Atmosphere& atmosphere, Clouds& clouds, TextureHandle weather);
	void DrawBloomControls(Bloom& bloom);
	void DrawRenderVisualizer(RenderDevice* device, ClusteredLightCulling& clusteredCulling, TextureHandle overlay);

	void AddConsoleMessage(const std::string& message);

	// Saves a copy of the texture to CPU-readback memory.
	void CaptureThumbnail(RenderDevice& device, CommandList& list, TextureHandle ldr);
};