// Copyright (c) 2019-2021 Andrew Depke

#include <Editor/EditorRenderer.h>
#include <Editor/EditorUI.h>

namespace EditorRenderer
{
	void Render(RenderDevice* device, entt::registry& registry, TextureHandle depthStencilTexture, TextureHandle sceneTexture)
	{
		EditorUI::Get().DrawLayout();
		EditorUI::Get().DrawDemoWindow();
		EditorUI::Get().DrawScene(device, sceneTexture);
		EditorUI::Get().DrawEntityHierarchy(registry);
		EditorUI::Get().DrawEntityPropertyViewer(registry);
		EditorUI::Get().DrawRenderGraph(device, depthStencilTexture, sceneTexture);
	}
}