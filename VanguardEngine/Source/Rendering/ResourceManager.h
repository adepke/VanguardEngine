// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <memory>

struct ResourceDescription;
struct GPUBuffer;

namespace D3D12MA
{
	class Allocator;
}

class ResourceManager
{
private:
	// #TODO: Vector of shared pointers to resources?
	// Idea: when a fence signal comes in telling us that a resource is no longer needed/finished uploading, we remove it from the pool here?

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

	std::shared_ptr<GPUBuffer> Allocate(ResourcePtr<D3D12MA::Allocator>& Allocator, const ResourceDescription& Description);
};