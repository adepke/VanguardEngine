// Copyright (c) 2019-2021 Andrew Depke

#include <Editor/EditorRenderer.h>
#include <Editor/EditorUI.h>

namespace EditorRenderer
{
	void Render(RenderDevice* device, entt::registry& registry, TextureHandle sceneTexture)
	{
		EditorUI::Get().DrawLayout();
		EditorUI::Get().DrawScene(device, sceneTexture);
		EditorUI::Get().DrawEntityHierarchy(registry);
		EditorUI::Get().DrawEntityPropertyViewer(registry);
	}
}