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

	capacity = maxMaterials;
	materialBuffer = device->GetResourceManager().Create(desc, VGText("Material table"));

	// Fill the material table with a plain white material that can be used while async loading materials.
	MaterialData placeholder{};
	placeholder.baseColorFactor = XMFLOAT4{ 1.f, 1.f, 1.f, 1.f };
	placeholder.metallicFactor = 0.f;
	placeholder.roughnessFactor = 1.f;

	std::vector<MaterialData> defaults(maxMaterials, placeholder);
	device->GetResourceManager().Write(materialBuffer, defaults);
}

size_t MaterialFactory::Create()
{
	VGAssert(count < capacity, "Material table is full, increase maxMaterials.");
	++pending;
	return count++;
}

void MaterialFactory::MarkLoaded()
{
	VGAssert(pending > 0, "More materials marked as loaded than were created.");
	--pending;
}
