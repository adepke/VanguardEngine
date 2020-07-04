// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Material.h>
#include <Utility/Singleton.h>

#include <vector>

class RenderDevice;

// #TODO: Remove this.
class MaterialManager : public Singleton<MaterialManager>
{
public:
	// Compiles shaders, builds pipelines.
	std::vector<Material> ReloadMaterials(RenderDevice& device);
};