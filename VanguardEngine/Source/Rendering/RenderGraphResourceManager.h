// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/RenderGraphResource.h>

#include <unordered_map>
#include <optional>

class RenderDevice;
class RenderGraph;

class RenderGraphResourceManager
{
private:
	size_t counter = 0;

	std::unordered_map<RenderResource, BufferHandle> bufferResources;
	std::unordered_map<RenderResource, TextureHandle> textureResources;

	std::unordered_map<RenderResource, std::pair<TransientBufferDescription, std::wstring>> transientBufferResources;
	std::unordered_map<RenderResource, std::pair<TransientTextureDescription, std::wstring>> transientTextureResources;

public:
	const RenderResource AddResource(const BufferHandle resource);
	const RenderResource AddResource(const TextureHandle resource);
	const RenderResource AddResource(const TransientBufferDescription& description, const std::wstring& name);
	const RenderResource AddResource(const TransientTextureDescription& description, const std::wstring& name);

	void BuildTransients(RenderDevice* device, RenderGraph* graph);

public:
	const BufferHandle GetBuffer(const RenderResource resource);
	const TextureHandle GetTexture(const RenderResource resource);

	std::optional<BufferHandle> GetOptionalBuffer(const RenderResource resource);
	std::optional<TextureHandle> GetOptionalTexture(const RenderResource resource);
};

inline const RenderResource RenderGraphResourceManager::AddResource(const BufferHandle resource)
{
	RenderResource result{ counter++ };
	bufferResources[result] = resource;

	return result;
}

inline const RenderResource RenderGraphResourceManager::AddResource(const TextureHandle resource)
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

inline const BufferHandle RenderGraphResourceManager::GetBuffer(const RenderResource resource)
{
	VGAssert(bufferResources.contains(resource), "Failed to get resource as a buffer.");
	return bufferResources[resource];
}

inline const TextureHandle RenderGraphResourceManager::GetTexture(const RenderResource resource)
{
	VGAssert(textureResources.contains(resource), "Failed to get resource as a texture.");
	return textureResources[resource];
}

inline std::optional<BufferHandle> RenderGraphResourceManager::GetOptionalBuffer(const RenderResource resource)
{
	return bufferResources.contains(resource) ? std::optional{ bufferResources[resource] } : std::nullopt;
}

inline std::optional<TextureHandle> RenderGraphResourceManager::GetOptionalTexture(const RenderResource resource)
{
	return textureResources.contains(resource) ? std::optional{ textureResources[resource] } : std::nullopt;
}