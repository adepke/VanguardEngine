// Copyright (c) 2019-2021 Andrew Depke

#include <Asset/AssetManager.h>
#include <Asset/AssetLoader.h>
#include <Rendering/Renderer.h>

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
			device->GetResourceManager().GenerateMipmaps(resource);
		}
		device->GetDirectList().TransitionBarrier(resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		return device->GetResourceManager().Get(resource).SRV->bindlessIndex;
	};

	std::vector<uint32_t> table{};
	table.resize(8);

	// #TODO: Include asset name in texture name.
	table[0] = CreateTexture(material.pbrMetallicRoughness.baseColorTexture.index, VGText("Base color asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true);
	table[1] = CreateTexture(material.pbrMetallicRoughness.metallicRoughnessTexture.index, VGText("Metallic roughness asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM, true);
	table[2] = CreateTexture(material.normalTexture.index, VGText("Normal asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM, true);
	table[3] = CreateTexture(material.occlusionTexture.index, VGText("Occlusion asset texture"), DXGI_FORMAT_R8_UNORM, false);
	table[4] = CreateTexture(material.emissiveTexture.index, VGText("Emissive asset texture"), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, false);  // #TODO: Correct format?
	table[5] = 0;
	table[6] = 0;
	table[7] = 0;

	std::vector<uint8_t> tableData{};
	tableData.resize(table.size() * sizeof(uint32_t));
	std::memcpy(tableData.data(), table.data(), tableData.size());

	device->GetResourceManager().Write(buffer, tableData);
	device->GetDirectList().TransitionBarrier(buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}