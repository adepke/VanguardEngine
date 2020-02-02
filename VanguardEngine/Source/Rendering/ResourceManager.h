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
	RenderDevice* Device;
	size_t FrameCount;
	
	std::vector<ResourcePtr<D3D12MA::Allocation>> UploadResources;
	std::vector<size_t> UploadOffsets;
	std::vector<void*> UploadPtrs;

	void CreateResourceViews(std::shared_ptr<Buffer>& Target);
	void CreateResourceViews(std::shared_ptr<Texture>& Target);
	void NameResource(ResourcePtr<D3D12MA::Allocation>& Target, const std::wstring_view Name);

public:
	ResourceManager() = default;
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager(ResourceManager&&) noexcept = delete;

	ResourceManager& operator=(const ResourceManager&) = delete;
	ResourceManager& operator=(ResourceManager&&) noexcept = delete;

	void Initialize(RenderDevice* Device, size_t BufferedFrames);

	std::shared_ptr<Buffer> AllocateBuffer(const BufferDescription& Description, const std::wstring_view Name);
	std::shared_ptr<Texture> AllocateTexture(const TextureDescription& Description, const std::wstring_view Name);
	
	// Creates a texture from the swap chain surface.
	std::shared_ptr<Texture> ResourceFromSwapChain(void* Surface, const std::wstring_view Name);

	// Source data can be discarded immediately.
	void WriteBuffer(std::shared_ptr<Buffer>& Target, const std::vector<uint8_t>& Source, size_t TargetOffset = 0);
	void WriteTexture(std::shared_ptr<Texture>& Target, const std::vector<uint8_t>& Source);

	void CleanupFrameResources(size_t Frame);
};