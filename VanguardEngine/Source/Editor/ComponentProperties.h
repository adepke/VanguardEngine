// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>

#include <entt/entt.hpp>

namespace ComponentProperties
{
	// Entity may be null, if the component is not owned. Registry and entity provided for cross-component or
	// cross-entity behavior in the render function. Returns true if the component was modified.

	// Core components.

	bool RenderNameComponent(NameComponent& component, entt::registry& registry, entt::entity entity);
	bool RenderTransformComponent(TransformComponent& component, entt::registry& registry, entt::entity entity);
	bool RenderControlComponent(ControlComponent& component, entt::registry& registry, entt::entity entity);

	// Rendering components.

	bool RenderMeshComponent(MeshComponent& component, entt::registry& registry, entt::entity entity);
	bool RenderCameraComponent(CameraComponent& component, entt::registry& registry, entt::entity entity);
	bool RenderLightComponent(LightComponent& component, entt::registry& registry, entt::entity entity);
	bool RenderTimeOfDayComponent(TimeOfDayComponent& component, entt::registry& registry, entt::entity entity);
	bool RenderWeatherComponent(WeatherComponent& component, entt::registry& registry, entt::entity entity);
}
