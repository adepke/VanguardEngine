// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>
#include <Rendering/DescriptorHeap.h>

#include <vector>
#include <optional>

struct TextureDescription : ResourceDescription
{
	uint32_t Width = 1;
	uint32_t Height = 1;
	uint32_t Depth = 1;
	DXGI_FORMAT Format;
};

struct Texture : Resource
{
	TextureDescription Description;

	std::optional<DescriptorHandle> RTV;
	std::optional<DescriptorHandle> DSV;
	std::optional<DescriptorHandle> SRV;
	std::vector<DescriptorHandle> UAV;
};