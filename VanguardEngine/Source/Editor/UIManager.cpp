// Copyright (c) 2019-2020 Andrew Depke

#include <Editor/UIManager.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>

#include <imgui.h>

void UIManager::DrawScene()
{
	ImGui::SetNextWindowSize({ 400, 300 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.f);

	ImGui::Begin("Scene");

	const auto WindowPos = ImGui::GetWindowPos();

	auto ContentMin = ImGui::GetWindowContentRegionMin();
	auto ContentMax = ImGui::GetWindowContentRegionMax();

	ContentMin.x += WindowPos.x;
	ContentMin.y += WindowPos.y;
	ContentMax.x += WindowPos.x;
	ContentMax.y += WindowPos.y;

	SceneViewport.PositionX = ContentMin.x;
	SceneViewport.PositionY = ContentMin.y;
	SceneViewport.Width = ContentMax.x - ContentMin.x;
	SceneViewport.Height = ContentMax.y - ContentMin.y;

	ImGui::End();
}

void UIManager::DrawEntityViewer(entt::registry& Registry)
{
	// #TODO: Use reflection to get all entity properties.

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
}