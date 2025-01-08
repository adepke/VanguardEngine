// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>

#include <entt/entt.hpp>

namespace ComponentProperties
{
	// Core components.

	void RenderNameComponent(entt::registry& registry, entt::entity entity);
	void RenderTransformComponent(entt::registry& registry, entt::entity entity);
	void RenderControlComponent(entt::registry& registry, entt::entity entity);

	// Rendering components.

	void RenderMeshComponent(entt::registry& registry, entt::entity entity);
	void RenderCameraComponent(entt::registry& registry, entt::entity entity);
	void RenderLightComponent(entt::registry& registry, entt::entity entity);
	void RenderTimeOfDayComponent(entt::registry& registry, entt::entity entity);
}