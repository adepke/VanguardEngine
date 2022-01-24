// Copyright (c) 2019-2022 Andrew Depke

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

	// Convert radians to degrees.
	for (auto& dimension : rotation)
	{
		dimension *= 180.f / 3.14159265359f;
	}

	ImGui::Text("Transform");

	ImGui::DragFloat3("Translation", translation, 1.f, -100000.0, 100000.0, "%.4f");
	ImGui::DragFloat3("Rotation", rotation, 0.5f, -360.0, 360.0, "%.4f");
	ImGui::DragFloat3("Scale", scale, 0.025f, -10000.0, 10000.0, "%.4f");

	// Convert degrees to radians.
	for (auto& dimension : rotation)
	{
		dimension *= 3.14159265359f / 180.f;
	}

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
	ImGui::Text("Subsets: %i", component.subsets.size());
	ImGui::Text("Vertex metadata");

	bool enabled = true;
	bool disabled = false;

	const auto isChannelActive = [&component](uint32_t shift)
	{
		return (component.metadata.activeChannels >> shift) & 0x1;
	};

	static_assert(vertexChannels == 6, "Editor out of date with vertex channels.");

	// #TODO: Read-only checkboxes.
	ImGui::Indent();
	ImGui::Checkbox("Position", isChannelActive(0) ? &enabled : &disabled);
	ImGui::Checkbox("Normal", isChannelActive(1) ? &enabled : &disabled);
	ImGui::Checkbox("Texcoord", isChannelActive(2) ? &enabled : &disabled);
	ImGui::Checkbox("Tangent", isChannelActive(3) ? &enabled : &disabled);
	ImGui::Checkbox("Bitangent", isChannelActive(4) ? &enabled : &disabled);
	ImGui::Checkbox("Color", isChannelActive(5) ? &enabled : &disabled);
	ImGui::Unindent();
}

void ComponentProperties::RenderCameraComponent(entt::registry& registry, entt::entity entity)
{
	auto& component = registry.get<CameraComponent>(entity);

	ImGui::Text("Camera");
}