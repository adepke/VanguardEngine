// Copyright (c) 2019-2020 Andrew Depke

#include <Core/CoreSystems.h>
#include <Core/CoreComponents.h>
#include <Rendering/Renderer.h>
#include <Window/WindowFrame.h>

#include <imgui.h>

void ControlSystem::Update(entt::registry& Registry)
{
	// If any entities have control.
	if (Registry.view<const ControlComponent>().size())
	{
		Renderer::Get().Window->RestrainCursor(CursorRestraint::ToCenter);
		Renderer::Get().Window->ShowCursor(false);

		// #TODO: Conditionally compile this out if we're not compiling with the editor.
		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
		{
			Registry.clear<ControlComponent>();  // Rescind all control, returning it to the editor.
		}
	}

	else
	{
		Renderer::Get().Window->RestrainCursor(CursorRestraint::None);
		Renderer::Get().Window->ShowCursor(true);

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))  // Left or right click gives control.
		{
			// GetHoveredViewport()
			{
				// #TEMP
				//entt::registry::entity_type SomeEntity;
				//Registry.emplace<ControlComponent>(SomeEntity);
			}
		}
	}
}