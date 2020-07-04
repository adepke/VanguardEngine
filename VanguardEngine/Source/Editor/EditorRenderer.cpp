// Copyright (c) 2019-2020 Andrew Depke

#include <Editor/EditorRenderer.h>
#include <Editor/EditorUI.h>

namespace EditorRenderer
{
	Viewport GetSceneViewport()
	{
		return EditorUI::Get().sceneViewport;
	}

	void Render(entt::registry& registry)
	{
		EditorUI::Get().DrawScene();
		EditorUI::Get().DrawToolbar();
		EditorUI::Get().DrawEntityHierarchy(registry);
		EditorUI::Get().DrawEntityPropertyViewer(registry);
		EditorUI::Get().DrawAssetBrowser();
	}
}