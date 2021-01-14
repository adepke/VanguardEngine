// Copyright (c) 2019-2021 Andrew Depke

#include <Editor/ComponentProperties.h>

#include <imgui.h>

#include <cstring>

void ComponentProperties::RenderNameComponent(entt::registry& registry, entt::entity entity)
{
	auto& component = registry.get<NameComponent>(entity);

	component.name.resize(256, 0);

	ImGui::Text("Name");
	ImGui::SameLine();
	ImGui::InputText("", component.name.data(), component.name.size(), ImGuiInputTextFlags_AutoSelectAll);

	component.name.resize(std::strlen(component.name.data()));
}

void ComponentProperties::RenderTransformComponent(entt::registry& registry, entt::entity entity)
{
	auto& component = registry.get<TransformComponent>(entity);

	float translation[] = { component.translation.x, component.translation.y, component.translation.z };
	float rotation[] = { component.rotation.x, component.rotation.y, component.rotation.z };
	float scale[] = { component.scale.x, component.scale.y, component.scale.z };

	ImGui::Text("Transform");

	ImGui::DragFloat3("Translation", translation, 1.0, -100000.0, 100000.0, "%.4f");
	ImGui::DragFloat3("Rotation", rotation, 1.0, -100.0, 100.0, "%.4f");
	ImGui::DragFloat3("Scale", scale, 0.2, -10000.0, 10000.0, "%.4f");

	component.translation = XMFLOAT3{ translation };
	component.rotation = XMFLOAT3{ rotation };
	component.scale = XMFLOAT3{ scale };
}

void ComponentProperties::RenderControlComponent(entt::registry& registry, entt::entity entity)
{
	ImGui::Text("This entity has control.");
}

void ComponentProperties::RenderMeshComponent(entt::registry& registry, entt::entity entity)
{
	auto& component = registry.get<MeshComponent>(entity);

	ImGui::Text("Mesh");
}

void ComponentProperties::RenderCameraComponent(entt::registry& registry, entt::entity entity)
{
	auto& component = registry.get<CameraComponent>(entity);

	ImGui::Text("Camera");
}