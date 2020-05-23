// Copyright (c) 2019-2020 Andrew Depke

#include <Editor/EditorRenderer.h>
#include <Editor/UIManager.h>

Viewport EditorRenderer::GetSceneViewport()
{
	return UIManager::Get().SceneViewport;
}

void EditorRenderer::Render(entt::registry& Registry)
{
	UIManager::Get().DrawScene();
	UIManager::Get().DrawEntityViewer(Registry);
}