// Copyright (c) 2019-2020 Andrew Depke

#include <Editor/UIManager.h>
//#include <Core/CoreComponents.h>  // #TEMP: Causing build errors.
#include <Rendering/RenderComponents.h>

#include <imgui.h>

void UIManager::DrawEntityViewer(entt::registry& Registry)
{
	// #TODO: Use reflection to get all entity properties.
	/*
	const auto EntityView = Registry.view<TransformComponent>();
	const auto EntityCount = EntityView.size();

	ImGui::Begin("Entity Viewer");
	ImGui::Text("%i Entities", EntityCount);
	
	EntityView.each([](auto Entity, auto& Transform)
		{
			if (ImGui::TreeNode((void*)Entity, "ID: %i", Entity))
			{
				ImGui::InputFloat("X", &Transform.Translation.x);
				ImGui::InputFloat("Y", &Transform.Translation.y);
				ImGui::InputFloat("Z", &Transform.Translation.z);

				ImGui::TreePop();
			}
		});

	ImGui::End();
	*/
}

void UIManager::Render(entt::registry& Registry)
{
	DrawEntityViewer(Registry);
}