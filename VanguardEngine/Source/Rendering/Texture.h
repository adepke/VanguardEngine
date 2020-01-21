// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <optional>

struct Texture2D : Resource
{
	uint32_t Width;
	uint32_t Height;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;

	DescriptorHandle SRV;
	std::optional<DescriptorHandle> RTV;
	std::optional<DescriptorHandle> DSV;
};

// #TODO: Implement Texture3D.