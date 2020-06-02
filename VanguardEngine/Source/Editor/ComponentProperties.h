// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>

#include <entt/entt.hpp>

namespace ComponentProperties
{
	// Core components.

	void RenderNameComponent(entt::registry& Registry, entt::entity Entity);
	void RenderTransformComponent(entt::registry& Registry, entt::entity Entity);
	void RenderControlComponent(entt::registry& Registry, entt::entity Entity);

	// Rendering components.

	void RenderMeshComponent(entt::registry& Registry, entt::entity Entity);
	void RenderCameraComponent(entt::registry& Registry, entt::entity Entity);
}