// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <D3D12MemAlloc.h>

#include <vector>
#include <memory>
#include <string_view>

class RenderDevice;
struct ResourceDescription;
struct GPUBuffer;

namespace D3D12MA
{
	class Allocation;
}

class ResourceManager
{
private:
	size_t FrameCount;
	size_t CurrentFrame = 0;
	std::vector<ResourcePtr<D3D12MA::Allocation>> UploadResources;
	std::vector<size_t> UploadOffsets;
	std::vector<void*> UploadPtrs;

	void CreateBindings(const std::shared_ptr<GPUBuffer>& Buffer, const ResourceDescription& Description);

public:
	ResourceManager() = default;
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager(ResourceManager&&) noexcept = delete;

	ResourceManager& operator=(const ResourceManager&) = delete;
	ResourceManager& operator=(ResourceManager&&) noexcept = delete;

	void Initialize(RenderDevice& Device, size_t BufferedFrames);

	std::shared_ptr<GPUBuffer> Allocate(RenderDevice& Device, const ResourceDescription& Description, const std::wstring_view Name);
	
	// Used for allocations from render API internal buffers such as the swap chain surface. Description is needed for bindings.
	std::shared_ptr<GPUBuffer> AllocateFromAPIBuffer(const ResourceDescription& Description, void* Buffer, const std::wstring_view Name);

	// Source data can be discarded immediately.
	void Write(RenderDevice& Device, std::shared_ptr<GPUBuffer>& Buffer, const std::vector<uint8_t>& Source, size_t BufferOffset = 0);

	void CleanupFrameResources(size_t Frame);
};