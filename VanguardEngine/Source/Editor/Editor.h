// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/RenderGraphResource.h>
#include <Utility/Singleton.h>

#include <entt/entt.hpp>

class EditorUI;
class RenderGraph;
class RenderDevice;
class Renderer;
struct ClusterResources;

class Editor : public Singleton<Editor>
{
private:
#if ENABLE_EDITOR
	bool enabled = true;
	std::unique_ptr<EditorUI> ui;  // Maintains all user interface state.
#endif

public:
	Editor();
	~Editor();

	void Update();
	void Render(RenderGraph& graph, RenderDevice& device, Renderer& renderer, entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil,
		RenderResource outputLDR, RenderResource backBuffer, const ClusterResources& clusterResources);
};