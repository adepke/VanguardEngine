// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/DescriptorHeap.h>

#include <vector>
#include <optional>

struct TextureDescription : ResourceDescription
{
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	DXGI_FORMAT format;
};

struct Texture : Resource
{
	TextureDescription description;

	std::optional<DescriptorHandle> RTV;
	std::optional<DescriptorHandle> DSV;
	std::optional<DescriptorHandle> SRV;
	std::vector<DescriptorHandle> UAV;
};