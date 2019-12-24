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

class ResourceManager
{
private:
	size_t CurrentFrame = 1;
	std::array<std::vector<std::unique_ptr<ResourceWriteType>>, RenderDevice::FrameCount> FrameResources;  // Per-frame CPU resources that need to be destroyed after the frame has finished.

public:
	static inline ResourceManager& Get() noexcept
	{
		static ResourceManager Singleton;
		return Singleton;
	}

	ResourceManager() = default;
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager(ResourceManager&&) noexcept = delete;

	ResourceManager& operator=(const ResourceManager&) = delete;
	ResourceManager& operator=(ResourceManager&&) noexcept = delete;

	std::shared_ptr<GPUBuffer> Allocate(RenderDevice& Device, ResourcePtr<D3D12MA::Allocator>& Allocator, const ResourceDescription& Description, const std::wstring_view Name);

	// Maintains a reference until the GPU is finished writing, do not worry about keeping the source data alive.
	void Write(RenderDevice& Device, std::shared_ptr<GPUBuffer>& Buffer, std::unique_ptr<ResourceWriteType>&& Source, size_t BufferOffset = 0);

	void CleanupFrameResources(size_t Frame);
};