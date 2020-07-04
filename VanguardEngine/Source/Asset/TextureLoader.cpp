// Copyright (c) 2019-2020 Andrew Depke

#include <Asset/TextureLoader.h>
#include <Rendering/Device.h>
#include <Rendering/Texture.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace AssetLoader
{
	std::shared_ptr<Texture> LoadTexture(RenderDevice& device, std::filesystem::path path)
	{
		VGScopedCPUStat("Load Texture");

		int pixelsX;
		int pixelsY;
		int componentsPerPixel;

		unsigned char* data = nullptr;

		{
			VGScopedCPUStat("STB Load");

			data = stbi_load(path.generic_string().c_str(), &pixelsX, &pixelsY, &componentsPerPixel, STBI_rgb_alpha);
		}

		if (!data)
		{
			VGLogError(Asset) << "Failed to load texture at '" << path.generic_wstring() << "'.";
			return {};
		}

		std::vector<uint8_t> dataResource;

		{
			VGScopedCPUStat("Copy");

			dataResource.resize(static_cast<size_t>(pixelsX)* static_cast<size_t>(pixelsY)* static_cast<size_t>(STBI_rgb_alpha));

			std::memcpy(dataResource.data(), data, dataResource.size());

			STBI_FREE(data);
		}

		TextureDescription description{};
		description.width = pixelsX;
		description.height = pixelsY;
		description.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		description.updateRate = ResourceFrequency::Static;
		description.bindFlags = BindFlag::ShaderResource;
		description.accessFlags = AccessFlag::CPUWrite;

		// #TODO: Derive name from asset name + texture type.
		auto textureResource = device.CreateResource(description, VGText("Asset Texture"));

		device.WriteResource(textureResource, dataResource);

		return std::move(textureResource);
	}
}