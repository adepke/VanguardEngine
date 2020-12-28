// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/RenderGraphResource.h>

#include <unordered_map>

struct Buffer;
struct Texture;
class RenderDevice;
class RenderGraph;

class RenderGraphResourceManager
{
private:
	size_t counter = 0;

	std::unordered_map<RenderResource, std::shared_ptr<Buffer>> bufferResources;
	std::unordered_map<RenderResource, std::shared_ptr<Texture>> textureResources;

	std::unordered_map<RenderResource, std::pair<TransientBufferDescription, std::wstring>> transientBufferResources;
	std::unordered_map<RenderResource, std::pair<TransientTextureDescription, std::wstring>> transientTextureResources;

public:
	const RenderResource AddResource(const std::shared_ptr<Buffer>& resource);
	const RenderResource AddResource(const std::shared_ptr<Texture>& resource);
	const RenderResource AddResource(const TransientBufferDescription& description, const std::wstring& name);
	const RenderResource AddResource(const TransientTextureDescription& description, const std::wstring& name);

	void BuildTransients(RenderDevice* device, RenderGraph* graph);

public:
	template <typename T>
	T& Get(const RenderResource resource);

	// #TEMP: These will be removed when the resource handle system is implemented.
	auto& FetchAsBuffer(const RenderResource resource);
	auto& FetchAsTexture(const RenderResource resource);
};

inline const RenderResource RenderGraphResourceManager::AddResource(const std::shared_ptr<Buffer>& resource)
{
	RenderResource result{ counter++ };
	bufferResources[result] = resource;

	return result;
}

inline const RenderResource RenderGraphResourceManager::AddResource(const std::shared_ptr<Texture>& resource)
{
	RenderResource result{ counter++ };
	textureResources[result] = resource;

	return result;
}

inline const RenderResource RenderGraphResourceManager::AddResource(const TransientBufferDescription& description, const std::wstring& name)
{
	RenderResource result{ counter++ };
	transientBufferResources[result] = std::make_pair(description, name);

	return result;
}

inline const RenderResource RenderGraphResourceManager::AddResource(const TransientTextureDescription& description, const std::wstring& name)
{
	RenderResource result{ counter++ };
	transientTextureResources[result] = std::make_pair(description, name);

	return result;
}

template <typename T>
inline T& RenderGraphResourceManager::Get(const RenderResource resource)
{
	if constexpr (std::is_same_v<T, Buffer>)
	{
		VGAssert(bufferResources.contains(resource), "Failed to get resource as a buffer.");
		return *bufferResources[resource];
	}

	else if constexpr (std::is_same_v<T, Texture>)
	{
		VGAssert(textureResources.contains(resource), "Failed to get resource as a texture.");
		return *textureResources[resource];
	}
}

inline auto& RenderGraphResourceManager::FetchAsBuffer(const RenderResource resource)
{
	return bufferResources[resource];
}

inline auto& RenderGraphResourceManager::FetchAsTexture(const RenderResource resource)
{
	return textureResources[resource];
}