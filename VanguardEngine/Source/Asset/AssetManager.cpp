// Copyright (c) 2019-2022 Andrew Depke

#include <Asset/AssetManager.h>
#include <Asset/AssetLoader.h>
#include <Asset/AssetComponents.h>
#include <Core/Base.h>
#include <Rendering/Renderer.h>
#include <Rendering/ShaderStructs.h>

MeshComponent AssetManager::LoadModel(const std::filesystem::path& path)
{
	// Find the canonical representation of the path.
	std::error_code ec;
	const auto canonical = std::filesystem::weakly_canonical(path, ec);
	const auto key = (ec ? path : canonical).generic_wstring();

	// Check cache for a pre-existing version, otherwise fall back to loading it from disk.
	if (const auto it = loadedMeshes.find(key); it != loadedMeshes.end())
	{
		return it->second;
	}

	MeshComponent component = AssetLoader::LoadMesh(*device, *Renderer::Get().meshFactory, path);
	loadedMeshes.emplace(key, component);

	return component;
}

void AssetManager::ResolveMeshes(entt::registry& registry)
{
	// Get all entities with an AssetComponent but without a MeshComponent. These entities are pending
	// load. #TODO: This won't work in the future when there's non-mesh assets, consider a flag in the
	// AssetComponent like "loaded"?
	const auto view = registry.view<AssetComponent>(entt::exclude<MeshComponent>);
	for (const auto entity : view)
	{
		const auto& asset = view.get<AssetComponent>(entity);
		// Prefer the relative, fall back to absolute.
		auto path = asset.relativePath;
		if (!std::filesystem::exists(path))
		{
			path = asset.absolutePath;
			if (!std::filesystem::exists(path))
			{
				VGLogWarning(logAsset, "Asset path '{}' not found for entity, skipping resolution.",
					path.generic_wstring());
				continue;
			}
		}

		registry.emplace<MeshComponent>(entity, LoadModel(path));
	}
}

size_t AssetManager::EnqueueMaterialLoad(const tinygltf::Material& material)
{
	VGAssert(models.size() > 0, "No models available to queue materials for.");

	const auto index = Renderer::Get().materialFactory->Create();

	if (newModel)
	{
		modelMaterialQueues.emplace_back();
		newModel = false;
	}
	modelMaterialQueues.back().emplace(std::make_pair(material, index));

	return index;
}

void AssetManager::Update()
{
	tinygltf::Model* model = nullptr;
	MaterialQueue* queue = nullptr;

	while (models.size() > 0)
	{
		queue = &modelMaterialQueues.front();
		if (queue->size() == 0)
		{
			models.pop_front();
			modelMaterialQueues.pop_front();
		}

		else
		{
			model = &models.front();
			break;
		}
	}

	if (!model)
		return;
	
	auto [material, bufferIndex] = queue->front();
	queue->pop();

	// Create a single material and upload it to the GPU.

	const auto CreateTexture = [&](int index, std::wstring_view name, DXGI_FORMAT format, bool mipmap) -> uint32_t
	{
		if (index < 0)
		{
			return 0;
		}

		const auto& texture = model->images[model->textures[index].source];

		TextureDescription description{
			.bindFlags = BindFlag::ShaderResource,
			.accessFlags = AccessFlag::CPUWrite,
			.width = (uint32_t)texture.width,
			.height = (uint32_t)texture.height,
			.format = format,
			.mipMapping = mipmap
		};
		auto resource = device->GetResourceManager().Create(description, name);
		device->GetResourceManager().Write(resource, texture.image);
		if (mipmap)
		{
			device->GetResourceManager().GenerateMipmaps(device->GetDirectList(), resource);
		}
		device->GetDirectList().TransitionBarrier(resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		return device->GetResourceManager().Get(resource).SRV->bindlessIndex;
	};

	MaterialData materialData;
	// #TODO: Include asset name in texture name.
	materialData.baseColor = CreateTexture(material.pbrMetallicRoughness.baseColorTexture.index, VGText("Base color asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true);
	materialData.metallicRoughness = CreateTexture(material.pbrMetallicRoughness.metallicRoughnessTexture.index, VGText("Metallic roughness asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM, true);
	materialData.normal = CreateTexture(material.normalTexture.index, VGText("Normal asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM, true);
	materialData.occlusion = CreateTexture(material.occlusionTexture.index, VGText("Occlusion asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM, false);
	materialData.emissive = CreateTexture(material.emissiveTexture.index, VGText("Emissive asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, false);
	materialData.emissiveFactor.x = static_cast<float>(material.emissiveFactor[0]);
	materialData.emissiveFactor.y = static_cast<float>(material.emissiveFactor[1]);
	materialData.emissiveFactor.z = static_cast<float>(material.emissiveFactor[2]);
	materialData.baseColorFactor.x = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]);
	materialData.baseColorFactor.y = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]);
	materialData.baseColorFactor.z = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]);
	materialData.baseColorFactor.w = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[3]);
	materialData.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
	materialData.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);

	const auto materialBuffer = Renderer::Get().materialFactory->materialBuffer;

	device->GetResourceManager().Write(materialBuffer, materialData, bufferIndex * sizeof(MaterialData));
}