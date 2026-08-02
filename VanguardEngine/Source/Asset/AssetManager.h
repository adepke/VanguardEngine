// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Utility/Singleton.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/ResourceHandle.h>

#include <entt/entt.hpp>
#include <tiny_gltf.h>

#include <filesystem>
#include <list>
#include <queue>
#include <utility>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <string>

class RenderDevice;

class AssetManager : public Singleton<AssetManager>
{
	using MaterialQueue = std::queue<std::pair<tinygltf::Material, size_t>>;

	struct TextureCacheHash
	{
		size_t operator()(const std::pair<int, uint32_t>& key) const
		{
			return std::hash<uint64_t>{}(((uint64_t)(uint32_t)key.first << 32) | key.second);
		}
	};

	// Maps a (textureIndex, textureFormat) to bindless index.
	using TextureCache = std::unordered_map<std::pair<int, uint32_t>, uint32_t, TextureCacheHash>;

private:
	RenderDevice* device;
	// #TODO: Poor solution, should rework this.
	std::list<tinygltf::Model> models;
	std::list<MaterialQueue> modelMaterialQueues;
	std::list<TextureCache> modelTextureCaches;
	// Cache of already loaded meshes, by asset path.
	std::unordered_map<std::wstring, MeshComponent> loadedMeshes;

public:
	void Initialize(RenderDevice* inDevice) { device = inDevice; }

	// Prepares a new model for loading.
	tinygltf::Model& BeginModel()
	{
		modelMaterialQueues.emplace_back();
		modelTextureCaches.emplace_back();
		return models.emplace_back();
	}

	// Blocking load of the mesh data, will load materials over time.
	MeshComponent LoadModel(const std::filesystem::path& path);

	// Identifies entities with unloaded AssetComponents and loads them.
	void ResolveMeshes(entt::registry& registry);

	// Instead of loading all model materials in one frame, stagger loading out over multiple frames.
	size_t EnqueueMaterialLoad(const tinygltf::Material& material);

	void Update();
};