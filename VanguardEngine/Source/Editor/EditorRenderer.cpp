// Copyright (c) 2019-2020 Andrew Depke

#include <Editor/EditorRenderer.h>
#include <Editor/EditorUI.h>

namespace EditorRenderer
{
	void Render(entt::registry& registry)
	{
		// #TODO: Docking layout is disabled until bindless is implemented, which will allow
		// render graph textures to be used with ImGui.
		//EditorUI::Get().DrawLayout();
		//EditorUI::Get().DrawScene();
		EditorUI::Get().DrawEntityHierarchy(registry);
		EditorUI::Get().DrawEntityPropertyViewer(registry);
	}
}