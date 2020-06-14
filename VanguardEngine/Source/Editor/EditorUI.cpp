// Copyright (c) 2019-2020 Andrew Depke

#include <Editor/EditorUI.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Editor/EntityReflection.h>

#include <imgui.h>

#include <iterator>

void EditorUI::DrawScene()
{
	ImGui::SetNextWindowSize({ 400, 300 }, ImGuiCond_FirstUseEver);  // First use prevents the viewport from snapping back to the set size.
	ImGui::SetNextWindowBgAlpha(0.f);

	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);

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

void EditorUI::DrawToolbar()
{
	ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_None);

	ImGui::End();
}

void EditorUI::DrawEntityHierarchy(entt::registry& Registry)
{
	entt::entity SelectedEntity = entt::null;

	ImGui::Begin("Entity Hierarchy", nullptr, ImGuiWindowFlags_None);
	ImGui::Text("%i Entities", Registry.size());
	ImGui::Separator();
	
	Registry.each([this, &Registry, &SelectedEntity](auto Entity)
		{
			ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_None;

			if (Entity == HierarchySelectedEntity)
				NodeFlags |= ImGuiTreeNodeFlags_Selected;

			bool NodeOpen = false;

			ImGui::PushID(static_cast<int32_t>(Entity));  // Use the entity as the ID.

			if (Registry.has<NameComponent>(Entity))
			{
				NodeOpen = ImGui::TreeNodeEx("EntityTreeNode", NodeFlags, Registry.get<NameComponent>(Entity).Name.c_str());
			}

			else
			{
				// Strip the version info from the entity, we only care about the actual ID.
				NodeOpen = ImGui::TreeNodeEx("EntityTreeNode", NodeFlags, "Entity_%i", Registry.entity(Entity));
			}

			if (ImGui::IsItemClicked())
			{
				SelectedEntity = Entity;
			}

			if (NodeOpen)
			{
				// #TODO: Draw entity children.

				ImGui::TreePop();
			}

			ImGui::PopID();
		});

	ImGui::End();

	// Check if it's valid first, otherwise deselecting will remove the property viewer.
	if (Registry.valid(SelectedEntity))
	{
		HierarchySelectedEntity = SelectedEntity;
	}
}

void EditorUI::DrawEntityPropertyViewer(entt::registry& Registry)
{
	ImGui::Begin("Property Viewer", nullptr, ImGuiWindowFlags_None);

	if (Registry.valid(HierarchySelectedEntity))
	{
		uint32_t ComponentCount = 0;

		for (auto& [MetaID, RenderFunction] : EntityReflection::ComponentMap)
		{
			entt::id_type MetaList[] = { MetaID };

			if (Registry.runtime_view(std::cbegin(MetaList), std::cend(MetaList)).contains(HierarchySelectedEntity))
			{
				++ComponentCount;

				RenderFunction(Registry, HierarchySelectedEntity);

				ImGui::Separator();
			}
		}

		if (ComponentCount == 0)
		{
			ImGui::Text("No components.");
		}
	}

	ImGui::End();
}

void EditorUI::DrawAssetBrowser()
{
	ImGui::Begin("Asset Browser", nullptr, ImGuiWindowFlags_None);

	ImGui::End();
}