// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Editor/ComponentProperties.h>

#include <entt/entt.hpp>

#include <map>
#include <functional>

namespace EntityReflection
{
	// #TODO: Look into std::reflect for automated component discovery and reflection.

	inline const std::map<entt::id_type, std::function<void(entt::registry&, entt::entity)>> componentMap = {
		{ entt::type_id<NameComponent>().hash(), &ComponentProperties::RenderNameComponent },
		{ entt::type_id<TransformComponent>().hash(), &ComponentProperties::RenderTransformComponent },
		{ entt::type_id<ControlComponent>().hash(), &ComponentProperties::RenderControlComponent },
		{ entt::type_id<MeshComponent>().hash(), &ComponentProperties::RenderMeshComponent },
		{ entt::type_id<CameraComponent>().hash(), &ComponentProperties::RenderCameraComponent }
	};
}