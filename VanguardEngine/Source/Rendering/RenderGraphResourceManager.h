// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/ResourceBind.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/ResourceView.h>
#include <Rendering/PipelineState.h>

#include <unordered_map>
#include <utility>
#include <list>
#include <vector>
#include <optional>
#include <algorithm>

class RenderDevice;
class RenderGraph;

struct TransientBuffer
{
	RenderResource resource;
	uint8_t counter = 1;
	uint32_t binds;
	TransientBufferDescription description;
};

struct TransientTexture
{
	RenderResource resource;
	uint8_t counter = 1;
	uint32_t binds;
	TransientTextureDescription description;
};

struct RenderPassViews
{
	std::unordered_map<RenderResource, ResourceView> views;
};

class RenderGraphResourceManager
{
	friend class RenderGraph;

public:
	bool transientReuse = true;

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

	std::unordered_map<size_t, RenderPassViews> passViews;

	std::unordered_map<size_t, PipelineState> passPipelines;

private:
	DescriptorHandle CreateDescriptorFromView(RenderDevice* device, const RenderResource resource, ShaderResourceViewDescription viewDesc);
	uint32_t GetDefaultDescriptor(RenderDevice* device, const RenderResource resource, ResourceBind bind);

public:
	const RenderResource AddResource(const BufferHandle resource);
	const RenderResource AddResource(const TextureHandle resource);
	const RenderResource AddResource(const TransientBufferDescription& description, const std::wstring& name);
	const RenderResource AddResource(const TransientTextureDescription& description, const std::wstring& name);

	void BuildTransients(RenderDevice* device, RenderGraph* graph);
	void BuildDescriptors(RenderDevice* device, RenderGraph* graph);
	void DiscardTransients(RenderDevice* device);
	void DiscardDescriptors(RenderDevice* device);
	void DiscardPipelines();

public:
	const uint32_t GetDescriptor(size_t passIndex, const RenderResource resource, const std::string& name);
	const DescriptorHandle& GetFullDescriptor(size_t passIndex, const RenderResource resource, const std::string& name);

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

inline const uint32_t RenderGraphResourceManager::GetDescriptor(size_t passIndex, const RenderResource resource, const std::string& name)
{
	VGAssert(passViews.contains(passIndex), "No descriptors requested by pass index %zu", passIndex);
	auto& passView = passViews[passIndex].views;
	VGAssert(passView.contains(resource), "No descriptors created for resource.");
	auto& descriptors = passView[resource].descriptorIndices;
	if (name.size() > 0)
		VGAssert(descriptors.contains(name), "Failed to get descriptor with name '%s' from resource.", name.data());
	else
		VGAssert(descriptors.contains(name), "Failed to get default descriptor from resource.");

	return descriptors[name];
}

inline const DescriptorHandle& RenderGraphResourceManager::GetFullDescriptor(size_t passIndex, const RenderResource resource, const std::string& name)
{
	VGAssert(name.length() > 0, "Full descriptors must be named.");
	VGAssert(passViews.contains(passIndex), "No descriptors requested by pass index %zu", passIndex);
	auto& passView = passViews[passIndex].views;
	VGAssert(passView.contains(resource), "No descriptors created for resource.");
	auto& descriptors = passView[resource].fullDescriptors;
	VGAssert(descriptors.contains(name), "Failed to get full descriptor with name '%s' from resource.", name.data());

	return descriptors[name];
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