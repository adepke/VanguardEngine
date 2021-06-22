// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Resource.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/PipelineState.h>

#include <D3D12MemAlloc.h>

#include <vector>
#include <memory>
#include <string_view>

class RenderDevice;

class ResourceManager
{
private:
	// #TODO: Weak pointer instead of raw pointer?
	RenderDevice* device;
	entt::registry registry;
	size_t frameCount = 0;
	
	std::vector<ResourcePtr<D3D12MA::Allocation>> uploadResources;
	std::vector<size_t> uploadOffsets;
	std::vector<void*> uploadPtrs;

	// Frame-temporary resources. Only persist for a single GPU frame.
	std::vector<std::vector<TextureHandle>> frameTextures;
	std::vector<std::vector<BufferHandle>> frameBuffers;
	std::vector<std::vector<DescriptorHandle>> frameDescriptors;

	void CreateResourceViews(BufferComponent& target);
	void CreateResourceViews(TextureComponent& target);
	void SetResourceName(ResourcePtr<D3D12MA::Allocation>& target, const std::wstring_view name);

	PipelineState mipmapPipeline;

	void CreateMipmapTools();

public:
	ResourceManager() = default;
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager(ResourceManager&&) noexcept = delete;

	ResourceManager& operator=(const ResourceManager&) = delete;
	ResourceManager& operator=(ResourceManager&&) noexcept = delete;

	void Initialize(RenderDevice* inDevice, size_t bufferedFrames);

	const BufferHandle Create(const BufferDescription& description, const std::wstring_view name);
	const TextureHandle Create(const TextureDescription& description, const std::wstring_view name);
	
	// Creates a texture from the swap chain surface.
	const TextureHandle CreateFromSwapChain(void* surface, const std::wstring_view name);

	void NameResource(const BufferHandle handle, const std::wstring_view name);
	void NameResource(const TextureHandle handle, const std::wstring_view name);

	bool Valid(const BufferHandle handle) const;
	bool Valid(const TextureHandle handle) const;

	BufferComponent& Get(BufferHandle handle);
	TextureComponent& Get(TextureHandle handle);

	// Source data can be discarded immediately. Offsets are in bytes.
	void Write(BufferHandle target, const std::vector<uint8_t>& source, size_t targetOffset = 0);
	void Write(TextureHandle target, const std::vector<uint8_t>& source);

	void Destroy(BufferHandle handle);
	void Destroy(TextureHandle handle);

	void GenerateMipmaps(TextureHandle texture);

	void AddFrameResource(size_t frameIndex, const BufferHandle handle);
	void AddFrameResource(size_t frameIndex, const TextureHandle handle);
	void AddFrameDescriptor(size_t frameIndex, DescriptorHandle handle);

	void CleanupFrameResources(size_t frame);
};

inline void ResourceManager::NameResource(const BufferHandle handle, const std::wstring_view name)
{
	SetResourceName(Get(handle).allocation, name);
}

inline void ResourceManager::NameResource(const TextureHandle handle, const std::wstring_view name)
{
	SetResourceName(Get(handle).allocation, name);
}

inline bool ResourceManager::Valid(const BufferHandle handle) const
{
	return registry.valid(handle.handle);
}

inline bool ResourceManager::Valid(const TextureHandle handle) const
{
	return registry.valid(handle.handle);
}

inline BufferComponent& ResourceManager::Get(BufferHandle handle)
{
	VGAssert(registry.valid(handle.handle), "Fetching invalid buffer handle.");

	return registry.get<BufferComponent>(handle.handle);
}

inline TextureComponent& ResourceManager::Get(TextureHandle handle)
{
	VGAssert(registry.valid(handle.handle), "Fetching invalid texture handle.");

	return registry.get<TextureComponent>(handle.handle);
}

inline void ResourceManager::Destroy(BufferHandle handle)
{
	VGAssert(registry.valid(handle.handle), "Destroying invalid buffer handle.");

	auto& component = Get(handle);
	if (component.CBV) component.CBV->Free();
	if (component.SRV) component.SRV->Free();
	if (component.UAV) component.UAV->Free();
	if (registry.valid(component.counterBuffer.handle)) Destroy(component.counterBuffer);

	registry.destroy(handle.handle);
}

inline void ResourceManager::Destroy(TextureHandle handle)
{
	VGAssert(registry.valid(handle.handle), "Destroying invalid texture handle.");

	auto& component = Get(handle);
	if (component.RTV) component.RTV->Free();
	if (component.DSV) component.DSV->Free();
	if (component.SRV) component.SRV->Free();

	registry.destroy(handle.handle);
}

inline void ResourceManager::AddFrameResource(size_t frameIndex, const BufferHandle handle)
{
	frameBuffers[frameIndex].emplace_back(handle);
}

inline void ResourceManager::AddFrameResource(size_t frameIndex, const TextureHandle handle)
{
	frameTextures[frameIndex].emplace_back(handle);
}

inline void ResourceManager::AddFrameDescriptor(size_t frameIndex, DescriptorHandle handle)
{
	frameDescriptors[frameIndex].emplace_back(std::move(handle));
}