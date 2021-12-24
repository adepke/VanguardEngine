// Copyright (c) 2019-2021 Andrew Depke

#include <Asset/AssetManager.h>
#include <Asset/AssetLoader.h>
#include <Rendering/Renderer.h>
#include <Rendering/ShaderStructs.h>

MeshComponent AssetManager::LoadModel(const std::filesystem::path& path)
{
	return AssetLoader::LoadMesh(*device, *Renderer::Get().meshFactory, path);
}

void AssetManager::EnqueueMaterialLoad(const tinygltf::Material& material, BufferHandle buffer)
{
	materialQueue.emplace(std::make_pair(material, buffer));
}

void AssetManager::Update()
{
	if (materialQueue.size() == 0)
	{
		return;
	}
	
	auto [material, buffer] = materialQueue.front();
	materialQueue.pop();

	// Create a single material and upload it to the GPU.

	const auto CreateTexture = [&](int index, std::wstring_view name, DXGI_FORMAT format, bool mipmap) -> uint32_t
	{
		if (index < 0)
		{
			return 0;
		}

		const auto& texture = model.images[model.textures[index].source];

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

	std::vector<uint8_t> materialBytes{};
	materialBytes.resize(sizeof(materialData));
	std::memcpy(materialBytes.data(), &materialData, materialBytes.size());

	device->GetResourceManager().Write(buffer, materialBytes);
	device->GetDirectList().TransitionBarrier(buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}