// Copyright (c) 2019-2020 Andrew Depke

#include <Core/CoreSystems.h>
#include <Core/CoreComponents.h>
#include <Window/WindowFrame.h>

#include <imgui.h>

#include <Core/Windows/WindowsMinimal.h>

void ControlSystem::Update(entt::registry& Registry)
{
	// If any entities have control.
	if (Registry.view<const ControlComponent>().size())
	{
		WindowFrame::Get().RestrainCursor(CursorRestraint::ToCenter);
		WindowFrame::Get().ShowCursor(false);

		// #TODO: Conditionally compile this out if we're not compiling with the editor.
		auto& IO = ImGui::GetIO();
		if (IO.KeysDown[VK_ESCAPE])
		{
			Registry.clear<ControlComponent>();  // Rescind all control, returning it to the editor.
		}
	}

	else
	{
		WindowFrame::Get().RestrainCursor(CursorRestraint::None);
		WindowFrame::Get().ShowCursor(true);

		auto& IO = ImGui::GetIO();
		if (IO.MouseDown[0] || IO.MouseDown[1])  // Left or right click gives control.
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