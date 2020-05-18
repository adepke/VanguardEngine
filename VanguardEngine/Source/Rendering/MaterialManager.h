// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Material.h>

#include <vector>

class RenderDevice;

class MaterialManager
{
private:
	void* CompilerModule = nullptr;

public:
	static inline MaterialManager& Get() noexcept
	{
		static MaterialManager Singleton;
		return Singleton;
	}

	MaterialManager();
	MaterialManager(const MaterialManager&) = delete;
	MaterialManager(MaterialManager&&) noexcept = delete;

	MaterialManager& operator=(const MaterialManager&) = delete;
	MaterialManager& operator=(MaterialManager&&) noexcept = delete;

	~MaterialManager();

	// Compiles shaders, builds pipelines.
	std::vector<Material> ReloadMaterials(RenderDevice& Device);
};