// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

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

	std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> RTV;
	std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> DSV;
	std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> SRV;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> UAV;
};