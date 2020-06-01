// Copyright (c) 2019-2020 Andrew Depke

#include <Editor/EditorRenderer.h>
#include <Editor/EditorUI.h>

namespace EditorRenderer
{
	Viewport GetSceneViewport()
	{
		return EditorUI::Get().SceneViewport;
	}

	void Render(entt::registry& Registry)
	{
		EditorUI::Get().DrawScene();
		EditorUI::Get().DrawEntityHierarchy(Registry);
		EditorUI::Get().DrawEntityPropertyViewer(Registry);
	}
}