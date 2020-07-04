// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Editor/ComponentProperties.h>

#include <entt/entt.hpp>

#include <map>
#include <functional>

namespace EntityReflection
{
	// #TODO: Look into std::reflect for automated component discovery and reflection.

	inline const std::map<entt::id_type, std::function<void(entt::registry&, entt::entity)>> componentMap = {
		{ entt::type_info<NameComponent>::id(), &ComponentProperties::RenderNameComponent },
		{ entt::type_info<TransformComponent>::id(), &ComponentProperties::RenderTransformComponent },
		{ entt::type_info<ControlComponent>::id(), &ComponentProperties::RenderControlComponent },
		{ entt::type_info<MeshComponent>::id(), &ComponentProperties::RenderMeshComponent },
		{ entt::type_info<CameraComponent>::id(), &ComponentProperties::RenderCameraComponent }
	};
}