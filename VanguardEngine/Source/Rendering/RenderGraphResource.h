// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <functional>
#include <type_traits>
#include <optional>

// Typed resource for compile time validation.
struct RenderResource
{
	size_t id;

	bool operator==(const RenderResource other) const
	{
		return id == other.id;
	}

	bool operator<(const RenderResource other) const
	{
		return id < other.id;
	}
};

namespace std
{
	template <>
	struct hash<RenderResource>
	{
		size_t operator()(const RenderResource resource) const
		{
			return hash<remove_cv_t<decltype(resource.id)>>{}(resource.id);
		}
	};
}

struct TransientBufferDescription
{
	ResourceFrequency updateRate = ResourceFrequency::Dynamic;
	size_t size;  // Element count. size * stride = bytes.
	size_t stride = 0;
	std::optional<DXGI_FORMAT> format;

	bool operator==(const TransientBufferDescription& other) const noexcept
	{
		return updateRate == other.updateRate && size == other.size && stride == other.stride && format == other.format;
	}
};

struct TransientTextureDescription
{
	uint32_t width = 0;  // Will match back buffer resolution if left at 0.
	uint32_t height = 0;  // Will match back buffer resolution if left at 0.
	uint32_t depth = 1;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

	bool operator==(const TransientTextureDescription& other) const noexcept
	{
		return width == other.width && height == other.height && depth == other.depth && format == other.format;
	}
};