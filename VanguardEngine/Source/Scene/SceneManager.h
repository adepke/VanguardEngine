// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/ResourceHandle.h>

#include <entt/entt.hpp>

#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>

class RenderDevice;

struct SceneMetadata
{
	std::string name;
	std::filesystem::path path;
	TextureHandle thumbnail;  // May be invalid if no thumbnail.
};

namespace Scene
{
	// Lists all scenes, from the configured settings. Thumbnails are uploaded to the GPU immediately.
	std::vector<SceneMetadata> List(RenderDevice& device);
	// Blocking load of an entity scene, into the given registry. Clears the registry.
	bool Load(entt::registry& registry, const std::filesystem::path& path);
	// Saves the current registry into the given scene file.
	bool Save(const entt::registry& registry, const std::filesystem::path& path);
	// Unloads a scene, clearing all entities.
	void Clear(entt::registry& registry);
};