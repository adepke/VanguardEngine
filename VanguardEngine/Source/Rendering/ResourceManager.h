// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Device.h>

#include <memory>
#include <string_view>
#include <array>
#include <vector>

//class RenderDevice;
struct ResourceDescription;
struct GPUBuffer;

namespace D3D12MA
{
	class Allocator;
}

struct ResourceAllocation
{
	ResourcePtr<ID3D12Resource> Resource;
	std::vector<uint8_t> Source;
};

class ResourceManager
{
private:
	size_t CurrentFrame = 0;
	std::array<std::vector<ResourceAllocation>, RenderDevice::FrameCount> CPUFrameResources;  // Per-frame CPU resources that need to be destroyed after the frame has finished.
	ResourcePtr<D3D12MA::Allocation> UploadResource;
	size_t UploadOffset = 0;
	void* UploadPtr = nullptr;

public:
	ResourceManager();
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager(ResourceManager&&) noexcept = delete;

	ResourceManager& operator=(const ResourceManager&) = delete;
	ResourceManager& operator=(ResourceManager&&) noexcept = delete;

	std::shared_ptr<GPUBuffer> Allocate(RenderDevice& Device, const ResourceDescription& Description, const std::wstring_view Name);

	// Source data can be discarded immediately.
	void Write(RenderDevice& Device, std::shared_ptr<GPUBuffer>& Buffer, std::vector<uint8_t>&& Source, size_t BufferOffset = 0);

	void CleanupFrameResources(size_t Frame);
};