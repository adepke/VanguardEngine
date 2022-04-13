// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/MaterialFactory.h>
#include <Rendering/Device.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/ShaderStructs.h>

MaterialFactory::MaterialFactory(RenderDevice* device, size_t maxMaterials)
{
	BufferDescription desc{
		.updateRate = ResourceFrequency::Static,
		.bindFlags = BindFlag::ShaderResource,
		.accessFlags = AccessFlag::CPUWrite,
		.size = maxMaterials,
		.stride = sizeof(MaterialData)
	};

	materialBuffer = device->GetResourceManager().Create(desc, VGText("Material table"));

	// Zero out the buffer to ensure that if we try and render a material which hasn't loaded yet, we
	// don't read from uninitialized descriptor indexes.

	std::vector<uint8_t> emptyBytes;
	emptyBytes.resize(maxMaterials * sizeof(MaterialData), 0);

	device->GetResourceManager().Write(materialBuffer, emptyBytes);
}

size_t MaterialFactory::Create()
{
	return count++;
}