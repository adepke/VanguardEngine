// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderGraphResource.h>

#include <unordered_map>
#include <utility>
#include <list>
#include <optional>
#include <algorithm>

class RenderDevice;
class RenderGraph;

struct TransientBuffer
{
	RenderResource resource;
	uint8_t counter = 1;
	TransientBufferDescription description;
};

struct TransientTexture
{
	RenderResource resource;
	uint8_t counter = 1;
	TransientTextureDescription description;
};

class RenderGraphResourceManager
{
private:
	size_t counter = 0;

	std::unordered_map<RenderResource, BufferHandle> bufferResources;
	std::unordered_map<RenderResource, TextureHandle> textureResources;

	// Resources in staging, not yet created.
	std::unordered_map<RenderResource, std::pair<TransientBufferDescription, std::wstring>> transientBufferResources;
	std::unordered_map<RenderResource, std::pair<TransientTextureDescription, std::wstring>> transientTextureResources;

	// Resources created transiently, can be reused across frames.
	std::list<TransientBuffer> transientBuffers;
	std::list<TransientTexture> transientTextures;

public:
	const RenderResource AddResource(const BufferHandle resource);
	const RenderResource AddResource(const TextureHandle resource);
	const RenderResource AddResource(const TransientBufferDescription& description, const std::wstring& name);
	const RenderResource AddResource(const TransientTextureDescription& description, const std::wstring& name);

	void BuildTransients(RenderDevice* device, RenderGraph* graph);
	void DiscardTransients(RenderDevice* device);

public:
	const BufferHandle GetBuffer(const RenderResource resource);
	const TextureHandle GetTexture(const RenderResource resource);

	std::optional<BufferHandle> GetOptionalBuffer(const RenderResource resource);
	std::optional<TextureHandle> GetOptionalTexture(const RenderResource resource);
};

inline const RenderResource RenderGraphResourceManager::AddResource(const BufferHandle resource)
{
	// #TODO: Resources can be re-imported, and this will just create a new entry to the same underlying resource, but with a different handle.
	// This is probably an issue, will need to figure something out eventually.

	RenderResource result{ counter++ };
	bufferResources[result] = resource;

	return result;
}

inline const RenderResource RenderGraphResourceManager::AddResource(const TextureHandle resource)
{
	// #TODO: See above todo.

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