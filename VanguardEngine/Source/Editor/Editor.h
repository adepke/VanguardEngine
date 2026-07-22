// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/RenderGraphResource.h>
#include <Utility/Singleton.h>

#include <entt/entt.hpp>
#include <imgui.h>

class EditorUI;
class RenderGraph;
class RenderDevice;
class Renderer;
class RenderGraphResourceManager;
struct ClusterResources;
struct CloudResources;

class Editor : public Singleton<Editor>
{
public:
	bool enabled = true;

private:
#if ENABLE_EDITOR
	std::unique_ptr<EditorUI> ui;  // Maintains all user interface state.
#endif

	// Map tracked keybinds to the pressed state.
	std::vector<std::tuple<ImGuiKey, bool, std::function<void()>>> keybinds;

public:
	Editor();
	~Editor();

	void Update(RenderDevice& device, entt::registry& registry);
	void Render(RenderGraph& graph, RenderDevice& device, Renderer& renderer, RenderGraphResourceManager& resourceManager, entt::registry& registry,
		RenderResource cameraBuffer, RenderResource depthStencil, RenderResource outputLDR, RenderResource backBuffer, const ClusterResources& clusterResources,
		const CloudResources& cloudResources);

	void BindKey(ImGuiKey key, std::function<void()> function);

	void LogMessage(const std::string& message);
};