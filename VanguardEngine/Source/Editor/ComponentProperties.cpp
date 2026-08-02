// Copyright (c) 2019-2022 Andrew Depke

#include <Editor/ComponentProperties.h>

#include <imgui.h>

#include <cstring>

bool ComponentProperties::RenderNameComponent(NameComponent& component, entt::registry& registry, entt::entity entity)
{
	component.name.resize(256, 0);

	const bool changed = ImGui::InputText("##Name", component.name.data(), component.name.size(), ImGuiInputTextFlags_AutoSelectAll);

	component.name.resize(std::strlen(component.name.data()));

	return changed;
}

bool ComponentProperties::RenderTransformComponent(TransformComponent& component, entt::registry& registry, entt::entity entity)
{
	float translation[] = { component.translation.x, component.translation.y, component.translation.z };
	float rotation[] = { component.rotation.x, component.rotation.y, component.rotation.z };
	float scale[] = { component.scale.x, component.scale.y, component.scale.z };

	// Convert radians to degrees.
	for (auto& dimension : rotation)
	{
		dimension *= 180.f / 3.14159265359f;
	}

	bool changed = ImGui::DragFloat3("Translation", translation, 1.f, -100000.0, 100000.0, "%.4f");
	changed |= ImGui::DragFloat3("Rotation", rotation, 0.5f, -360.0, 360.0, "%.4f");
	changed |= ImGui::DragFloat3("Scale", scale, 0.025f, -10000.0, 10000.0, "%.4f");

	// Convert degrees to radians.
	for (auto& dimension : rotation)
	{
		dimension *= 3.14159265359f / 180.f;
	}

	if (changed)
	{
		component.translation = XMFLOAT3{ translation };
		component.rotation = XMFLOAT3{ rotation };
		component.scale = XMFLOAT3{ scale };
	}

	return changed;
}

bool ComponentProperties::RenderControlComponent(ControlComponent& component, entt::registry& registry, entt::entity entity)
{
	ImGui::Text("This entity has control.");

	return false;
}

bool ComponentProperties::RenderMeshComponent(MeshComponent& component, entt::registry& registry, entt::entity entity)
{
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

	return false;
}

bool ComponentProperties::RenderCameraComponent(CameraComponent& component, entt::registry& registry, entt::entity entity)
{
	bool changed = ImGui::DragFloat("Near plane", &component.nearPlane, 0.01f, 0.001f, component.farPlane, "%.3f");
	changed |= ImGui::DragFloat("Far plane", &component.farPlane, 1.f, component.nearPlane, 100000.f, "%.1f");

	float fieldOfViewDegrees = component.fieldOfView * 180.f / 3.14159265359f;  // Radians to degrees.
	if (ImGui::DragFloat("Field of view", &fieldOfViewDegrees, 0.5f, 1.f, 179.f, "%.1f"))
	{
		component.fieldOfView = fieldOfViewDegrees * 3.14159265359f / 180.f;
		changed = true;
	}

	return changed;
}

bool ComponentProperties::RenderLightComponent(LightComponent& component, entt::registry& registry, entt::entity entity)
{
	const char* lightTypes[] = { "Point", "Directional" };
	bool changed = ImGui::Combo("Light type", (int*)&component.type, lightTypes, std::size(lightTypes));

	changed |= ImGui::InputFloat3("Color", (float*)&component.color);

	return changed;
}

bool ComponentProperties::RenderTimeOfDayComponent(TimeOfDayComponent& component, entt::registry& registry, entt::entity entity)
{
	constexpr float maxZenithAngle = 3.14159f;
	bool changed = ImGui::DragFloat("Solar zenith angle", &component.solarZenithAngle, 0.005f, -maxZenithAngle, maxZenithAngle);
	changed |= ImGui::DragFloat("Speed", &component.speed, 0.01f, -10.f, 10.f);

	const char* animationTypes[] = { "Static", "Cycle", "Oscillate" };
	changed |= ImGui::Combo("Animation", (int*)&component.animation, animationTypes, std::size(animationTypes));

	return changed;
}

bool ComponentProperties::RenderWeatherComponent(WeatherComponent& component, entt::registry& registry, entt::entity entity)
{
	bool changed = ImGui::DragFloat("Cloud coverage", &component.coverage, 0.005f, 0.f, 1.f);
	changed |= ImGui::DragFloat("Precipitation", &component.precipitation, 0.005f, 0.f, 1.f);
	changed |= ImGui::DragFloat("Wind strength", &component.windStrength, 0.01f, 0.f, 1.f);
	changed |= ImGui::DragFloat2("Wind direction", (float*)&component.windDirection, 0.01f, -1.f, 1.f);

	return changed;
}
