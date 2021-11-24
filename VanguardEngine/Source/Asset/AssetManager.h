// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Utility/Singleton.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/ResourceHandle.h>

#include <tiny_gltf.h>

#include <filesystem>
#include <queue>
#include <utility>

class RenderDevice;

class AssetManager : public Singleton<AssetManager>
{
private:
	RenderDevice* device;
	std::queue<std::pair<tinygltf::Material, BufferHandle>> materialQueue;

public:
	// Need to keep the model data alive for delayed material loading.
	// #TODO: This is very temporary, will need a new system for multiple meshes.
	tinygltf::Model model;

public:
	void Initialize(RenderDevice* inDevice) { device = inDevice; }

	// Blocking load of the mesh data, will load materials over time.
	MeshComponent LoadModel(const std::filesystem::path& path);

	// Instead of loading all model materials in one frame, stagger loading out over multiple frames.
	void EnqueueMaterialLoad(const tinygltf::Material& material, BufferHandle buffer);

	void Update();
};