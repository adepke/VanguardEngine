// Copyright (c) 2019-2020 Andrew Depke

#include <Asset/TextureLoader.h>
#include <Rendering/Device.h>
#include <Rendering/Texture.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace AssetLoader
{
	std::shared_ptr<Texture> LoadTexture(RenderDevice& Device, std::filesystem::path Path)
	{
		VGScopedCPUStat("Load Texture");

		int PixelsX;
		int PixelsY;
		int ComponentsPerPixel;

		unsigned char* Data = nullptr;

		{
			VGScopedCPUStat("STB Load");

			Data = stbi_load(Path.generic_string().c_str(), &PixelsX, &PixelsY, &ComponentsPerPixel, STBI_rgb_alpha);
		}

		if (!Data)
		{
			VGLogError(Asset) << "Failed to load texture at '" << Path.generic_wstring() << "'.";
			return {};
		}

		std::vector<uint8_t> DataResource;

		{
			VGScopedCPUStat("Copy");

			DataResource.resize(static_cast<size_t>(PixelsX)* static_cast<size_t>(PixelsY)* static_cast<size_t>(STBI_rgb_alpha));

			std::memcpy(DataResource.data(), Data, DataResource.size());

			STBI_FREE(Data);
		}

		TextureDescription Description{};
		Description.Width = PixelsX;
		Description.Height = PixelsY;
		Description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		Description.UpdateRate = ResourceFrequency::Static;
		Description.BindFlags = BindFlag::ShaderResource;
		Description.AccessFlags = AccessFlag::CPUWrite;

		// #TODO: Derive name from asset name + texture type.
		auto TextureResource = Device.CreateResource(Description, VGText("Asset Texture"));

		Device.WriteResource(TextureResource, DataResource);

		return std::move(TextureResource);
	}
}