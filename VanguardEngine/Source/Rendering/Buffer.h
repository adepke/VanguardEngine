// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <optional>

struct Buffer : Resource
{
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;

	std::optional<DescriptorHandle> UAV;
	std::optional<DescriptorHandle> SRV;
};