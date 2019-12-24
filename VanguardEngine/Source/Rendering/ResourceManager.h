// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <memory>
#include <string_view>

class RenderDevice;
struct ResourceDescription;
struct GPUBuffer;

namespace D3D12MA
{
	class Allocator;
}

class ResourceManager
{
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
};