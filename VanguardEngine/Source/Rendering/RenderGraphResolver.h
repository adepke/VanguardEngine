// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Rendering/RenderGraphResource.h>

#include <unordered_map>
#include <memory>
#include <utility>
#include <type_traits>

class RenderDevice;

// Helper type to resolve resource tags into actual resource handles.
class RGResolver
{
private:
	size_t counter = 0;
	std::unordered_map<size_t, std::shared_ptr<Buffer>> bufferResources;
	std::unordered_map<size_t, std::shared_ptr<Texture>> textureResources;

	std::unordered_map<size_t, std::pair<RGBufferDescription, std::wstring>> transientBufferResources;
	std::unordered_map<size_t, std::pair<RGTextureDescription, std::wstring>> transientTextureResources;

public:  // Utilities for the render graph.
	size_t AddResource(const std::shared_ptr<Buffer>& resource) { const auto tag = counter++; bufferResources[tag] = resource; return tag; }
	size_t AddResource(const std::shared_ptr<Texture>& resource) { const auto tag = counter++; textureResources[tag] = resource; return tag; }

	size_t AddTransientResource(const RGBufferDescription& description) { const auto tag = counter++; transientBufferResources[tag].first = description; return tag; }
	size_t AddTransientResource(const RGTextureDescription& description) { const auto tag = counter++; transientTextureResources[tag].first = description; return tag; }

	void NameTransientBuffer(size_t resourceTag, const std::wstring& name) { transientBufferResources[resourceTag].second = name; }
	void NameTransientTexture(size_t resourceTag, const std::wstring& name) { transientTextureResources[resourceTag].second = name; }

	void BuildTransients(RenderDevice* device, std::unordered_map<size_t, ResourceDependencyData>& dependencies, std::unordered_map<size_t, ResourceUsageData>& usages);

	D3D12_RESOURCE_STATES DetermineInitialState(size_t resourceTag);

	inline std::shared_ptr<Buffer> FetchAsBuffer(size_t resourceTag);
	inline std::shared_ptr<Texture> FetchAsTexture(size_t resourceTag);

public:
	template <typename T>
	T& Get(size_t resourceTag)
	{
		if constexpr (std::is_same_v<T, Buffer>)
		{
			VGAssert(bufferResources.count(resourceTag), "Failed to resolve the resource as a buffer.");		
			return *bufferResources[resourceTag];
		}

		else if constexpr (std::is_same_v<T, Texture>)
		{
			VGAssert(textureResources.count(resourceTag), "Failed to resolve the resource as a texture.");
			return *textureResources[resourceTag];
		}

		else
		{
			static_assert(false, "Graph resolver can only accept types 'Buffer' or 'Texture'");
			return {};
		}
	}
};

inline std::shared_ptr<Buffer> RGResolver::FetchAsBuffer(size_t resourceTag)
{
	if (bufferResources.count(resourceTag))
		return bufferResources[resourceTag];

	return {};
}

inline std::shared_ptr<Texture> RGResolver::FetchAsTexture(size_t resourceTag)
{
	if (textureResources.count(resourceTag))
		return textureResources[resourceTag];

	return {};
}