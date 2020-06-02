// Copyright (c) 2019-2020 Andrew Depke

#include <Editor/ComponentProperties.h>

#include <imgui.h>

#include <cstring>

void ComponentProperties::RenderNameComponent(entt::registry& Registry, entt::entity Entity)
{
	auto& Component = Registry.get<NameComponent>(Entity);

	Component.Name.resize(256, 0);

	ImGui::Text("Name");
	ImGui::SameLine();
	ImGui::InputText("", Component.Name.data(), Component.Name.size(), ImGuiInputTextFlags_AutoSelectAll);

	Component.Name.resize(std::strlen(Component.Name.data()));
}

void ComponentProperties::RenderTransformComponent(entt::registry& Registry, entt::entity Entity)
{
	auto& Component = Registry.get<TransformComponent>(Entity);

	float Translation[] = { Component.Translation.x, Component.Translation.y, Component.Translation.z };
	float Rotation[] = { Component.Rotation.x, Component.Rotation.y, Component.Rotation.z };
	float Scale[] = { Component.Scale.x, Component.Scale.y, Component.Scale.z };

	ImGui::Text("Transform");

	ImGui::DragFloat3("Translation", Translation, 1.0, -100000.0, 100000.0, "%.4f");
	ImGui::DragFloat3("Rotation", Rotation, 1.0, -100.0, 100.0, "%.4f");
	ImGui::DragFloat3("Scale", Scale, 0.2, -10000.0, 10000.0, "%.4f");

	Component.Translation = XMFLOAT3{ Translation };
	Component.Rotation = XMFLOAT3{ Rotation };
	Component.Scale = XMFLOAT3{ Scale };
}

void ComponentProperties::RenderControlComponent(entt::registry& Registry, entt::entity Entity)
{
	ImGui::Text("This entity has control.");
}

void ComponentProperties::RenderMeshComponent(entt::registry& Registry, entt::entity Entity)
{
	auto& Component = Registry.get<MeshComponent>(Entity);

	ImGui::Text("Mesh");
}

void ComponentProperties::RenderCameraComponent(entt::registry& Registry, entt::entity Entity)
{
	auto& Component = Registry.get<CameraComponent>(Entity);

	ImGui::Text("Camera");
}