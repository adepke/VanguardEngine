// Copyright (c) 2019-2021 Andrew Depke

#include <Editor/EditorUI.h>
#include <Rendering/Device.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Editor/EntityReflection.h>

#include <imgui.h>

#include <iterator>

void EditorUI::DrawLayout()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });

	auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetWorkPos());
	ImGui::SetNextWindowSize(viewport->GetWorkSize());
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::Begin("Dock Space", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking);

	const auto dockSpaceId = ImGui::GetID("DockSpace");
	ImGui::DockSpace(dockSpaceId, { 0.f, 0.f }, ImGuiDockNodeFlags_None);

	ImGui::End();

	ImGui::PopStyleVar(3);
}

void EditorUI::DrawScene(RenderDevice* device, TextureHandle sceneTexture)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });  // Remove window padding.

	ImGui::SetNextWindowSize({ 400, 300 }, ImGuiCond_FirstUseEver);  // First use prevents the viewport from snapping back to the set size.
	ImGui::SetNextWindowBgAlpha(0.f);

	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);

	const auto& sceneTextureComponent = device->GetResourceManager().Get(sceneTexture);
	ImGui::Image((ImTextureID)sceneTextureComponent.SRV->bindlessIndex, { (float)sceneTextureComponent.description.width, (float)sceneTextureComponent.description.height });

	ImGui::End();

	ImGui::PopStyleVar();
}

void EditorUI::DrawEntityHierarchy(entt::registry& registry)
{
	entt::entity selectedEntity = entt::null;

	ImGui::Begin("Entity Hierarchy", nullptr, ImGuiWindowFlags_None);
	ImGui::Text("%i Entities", registry.size());
	ImGui::Separator();
	
	registry.each([this, &registry, &selectedEntity](auto entity)
		{
			ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_None;

			if (entity == hierarchySelectedEntity)
				nodeFlags |= ImGuiTreeNodeFlags_Selected;

			bool nodeOpen = false;

			ImGui::PushID(static_cast<int32_t>(entity));  // Use the entity as the ID.

			if (registry.has<NameComponent>(entity))
			{
				nodeOpen = ImGui::TreeNodeEx("EntityTreeNode", nodeFlags, registry.get<NameComponent>(entity).name.c_str());
			}

			else
			{
				// Strip the version info from the entity, we only care about the actual ID.
				nodeOpen = ImGui::TreeNodeEx("EntityTreeNode", nodeFlags, "Entity_%i", registry.entity(entity));
			}

			if (ImGui::IsItemClicked())
			{
				selectedEntity = entity;
			}

			if (nodeOpen)
			{
				// #TODO: Draw entity children.

				ImGui::TreePop();
			}

			ImGui::PopID();
		});

	ImGui::End();

	// Check if it's valid first, otherwise deselecting will remove the property viewer.
	if (registry.valid(selectedEntity))
	{
		hierarchySelectedEntity = selectedEntity;
	}
}

void EditorUI::DrawEntityPropertyViewer(entt::registry& registry)
{
	ImGui::Begin("Property Viewer", nullptr, ImGuiWindowFlags_None);

	if (registry.valid(hierarchySelectedEntity))
	{
		uint32_t componentCount = 0;

		for (auto& [metaID, renderFunction] : EntityReflection::componentMap)
		{
			entt::id_type metaList[] = { metaID };

			if (registry.runtime_view(std::cbegin(metaList), std::cend(metaList)).contains(hierarchySelectedEntity))
			{
				++componentCount;

				renderFunction(registry, hierarchySelectedEntity);

				ImGui::Separator();
			}
		}

		if (componentCount == 0)
		{
			ImGui::Text("No components.");
		}
	}

	ImGui::End();
}