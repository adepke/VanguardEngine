// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <D3D12MemAlloc.h>

#include <vector>
#include <memory>
#include <string_view>
#include <type_traits>

class RenderDevice;
struct Resource;
struct Buffer;
struct Texture;
struct ResourceDescription;
struct BufferDescription;
struct TextureDescription;

class ResourceManager
{
private:
	// #TODO: Weak pointer instead of raw pointer?
	RenderDevice* device = nullptr;
	size_t frameCount = 0;
	
	std::vector<ResourcePtr<D3D12MA::Allocation>> uploadResources;
	std::vector<size_t> uploadOffsets;
	std::vector<void*> uploadPtrs;

	void CreateResourceViews(std::shared_ptr<Buffer>& target);
	void CreateResourceViews(std::shared_ptr<Texture>& target);
	void NameResource(ResourcePtr<D3D12MA::Allocation>& target, const std::wstring_view name);

public:
	ResourceManager() = default;
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager(ResourceManager&&) noexcept = delete;

	ResourceManager& operator=(const ResourceManager&) = delete;
	ResourceManager& operator=(ResourceManager&&) noexcept = delete;

	void Initialize(RenderDevice* inDevice, size_t bufferedFrames);

	std::shared_ptr<Buffer> AllocateBuffer(const BufferDescription& description, const std::wstring_view name);
	std::shared_ptr<Texture> AllocateTexture(const TextureDescription& description, const std::wstring_view name);
	
	// Creates a texture from the swap chain surface.
	std::shared_ptr<Texture> TextureFromSwapChain(void* surface, const std::wstring_view name);

	// Source data can be discarded immediately. Offsets are in bytes.
	void WriteBuffer(std::shared_ptr<Buffer>& target, const std::vector<uint8_t>& source, size_t targetOffset = 0);
	void WriteTexture(std::shared_ptr<Texture>& target, const std::vector<uint8_t>& source);

	void CleanupFrameResources(size_t frame);
};