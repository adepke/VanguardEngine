// Copyright (c) 2019-2022 Andrew Depke

#include <Core/CoreSystems.h>
#include <Core/CoreComponents.h>
#include <Rendering/Renderer.h>
#include <Window/WindowFrame.h>

#include <imgui.h>

void ControlSystem::Update(entt::registry& registry)
{
	VGScopedCPUStat("Control System");

	// If any entities have control.
	if (registry.view<const ControlComponent>().size())
	{
		Renderer::Get().window->RestrainCursor(CursorRestraint::ToCenter);
		Renderer::Get().window->ShowCursor(false);

		// #TODO: Conditionally compile this out if we're not compiling with the editor.
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			registry.clear<ControlComponent>();  // Rescind all control, returning it to the editor.
		}
	}

	else
	{
		Renderer::Get().window->RestrainCursor(CursorRestraint::None);
		Renderer::Get().window->ShowCursor(true);
	}
}