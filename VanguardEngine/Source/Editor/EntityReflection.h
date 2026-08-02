// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Editor/ComponentProperties.h>
#include <Scene/Serialize.h>  // Must come after components for proper JSON detection!

#include <entt/entt.hpp>
#include <json.hpp>

#include <unordered_map>
#include <utility>
#include <type_traits>

namespace EntityReflection
{
	// Type-erased info about entity components. Reuses EnTT type erasure when possible.
	// #TODO: Look into std::reflect for automated component discovery and reflection.
	struct ComponentInfo
	{
		const char* name;

		// Renders the component and patches the entity if changed (and owned).
		void (*render)(void* component, entt::registry& registry, entt::entity entity);

		entt::any (*makeDefault)();

		// Adds a component to an entity.
		void (*add)(entt::any&& component, entt::registry& registry, entt::entity entity);

		// Internal components are managed by the engine and cannot be added or removed by hand.
		bool internal;
	};

	namespace Detail
	{
		// Checks if the given component has JSON reflection. If it doesn't it probably doesn't make sense
		// to allow it to be added to entities by hand in the editor.
		template <typename T>
		concept JsonReflected = requires(nlohmann::json & json, const T & value)
		{
			json = value;
		};

		static_assert(JsonReflected<TransformComponent>, "Expected TransformComponent to have JSON reflection, internal component detection may be broken.");

		template <typename T, bool (*Render)(T&, entt::registry&, entt::entity)>
		void RenderComponent(void* component, entt::registry& registry, entt::entity entity)
		{
			// Tags have no storage so just make a default one.
			if constexpr (std::is_empty_v<T>)
			{
				T tag{};
				Render(tag, registry, entity);
			}

			else
			{
				if (Render(*static_cast<T*>(component), registry, entity) && entity != entt::null)
				{
					registry.patch<T>(entity);
				}
			}
		}

		template <typename T>
		entt::any MakeDefault()
		{
			return entt::any{ std::in_place_type<T> };
		}

		template <typename T>
		void Add(entt::any&& component, entt::registry& registry, entt::entity entity)
		{
			if constexpr (std::is_empty_v<T>)
			{
				registry.emplace_or_replace<T>(entity);
			}

			else
			{
				registry.emplace_or_replace<T>(entity, std::move(*entt::any_cast<T>(&component)));
			}
		}

		template <typename T, bool (*Render)(T&, entt::registry&, entt::entity)>
		std::pair<entt::id_type, ComponentInfo> MakeComponentInfo(const char* name)
		{
			return { entt::type_id<T>().hash(), { name, &RenderComponent<T, Render>, &MakeDefault<T>, &Add<T>, !JsonReflected<T> }};
		}
	}

	inline const std::unordered_map<entt::id_type, ComponentInfo> componentMeta = {
		Detail::MakeComponentInfo<NameComponent, &ComponentProperties::RenderNameComponent>("Name"),
		Detail::MakeComponentInfo<TransformComponent, &ComponentProperties::RenderTransformComponent>("Transform"),
		Detail::MakeComponentInfo<ControlComponent, &ComponentProperties::RenderControlComponent>("Control"),
		Detail::MakeComponentInfo<MeshComponent, &ComponentProperties::RenderMeshComponent>("Mesh"),
		Detail::MakeComponentInfo<CameraComponent, &ComponentProperties::RenderCameraComponent>("Camera"),
		Detail::MakeComponentInfo<LightComponent, &ComponentProperties::RenderLightComponent>("Light"),
		Detail::MakeComponentInfo<TimeOfDayComponent, &ComponentProperties::RenderTimeOfDayComponent>("Time of Day"),
		Detail::MakeComponentInfo<WeatherComponent, &ComponentProperties::RenderWeatherComponent>("Weather")
	};
}
