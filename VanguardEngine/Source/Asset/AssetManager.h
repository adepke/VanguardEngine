// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Utility/Singleton.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/ResourceHandle.h>

#include <tiny_gltf.h>

#include <filesystem>
#include <list>
#include <queue>
#include <utility>

class RenderDevice;

class AssetManager : public Singleton<AssetManager>
{
	using MaterialQueue = std::queue<std::pair<tinygltf::Material, BufferHandle>>;

private:
	RenderDevice* device;
	std::list<MaterialQueue> modelMaterialQueues;

public:
	// #TODO: Poor solution, should rework this.
	std::list<tinygltf::Model> models;
	bool newModel = false;

public:
	void Initialize(RenderDevice* inDevice) { device = inDevice; }

	// Blocking load of the mesh data, will load materials over time.
	MeshComponent LoadModel(const std::filesystem::path& path);

	// Instead of loading all model materials in one frame, stagger loading out over multiple frames.
	void EnqueueMaterialLoad(const tinygltf::Material& material, BufferHandle buffer);

	void Update();
};