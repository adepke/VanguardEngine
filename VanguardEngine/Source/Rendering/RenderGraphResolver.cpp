// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraphResolver.h>
#include <Rendering/Device.h>

void RGResolver::BuildTransients(RenderDevice* device, std::unordered_map<size_t, ResourceDependencyData>& dependencies, std::unordered_map<size_t, ResourceUsageData>& usages)
{
	VGScopedCPUStat("Render Graph Build Transients");

	for (const auto& [tag, description] : transientBufferResources)
	{
		bool written = dependencies[tag].writingPasses.size();  // Is the resource ever written to.

		BufferDescription fullDescription{};
		fullDescription.updateRate = description.first.updateRate;
		fullDescription.bindFlags = BindFlag::ShaderResource;
		fullDescription.accessFlags = written ? AccessFlag::GPUWrite : 0;
		fullDescription.size = description.first.size;
		fullDescription.stride = description.first.stride;
		fullDescription.format = description.first.format;

		if (description.first.bufferTypeFlags & RGBufferTypeFlag::ConstantBuf) fullDescription.bindFlags |= BindFlag::ConstantBuffer;
		if (description.first.bufferTypeFlags & RGBufferTypeFlag::VertexBuf) fullDescription.bindFlags |= BindFlag::VertexBuffer;
		if (description.first.bufferTypeFlags & RGBufferTypeFlag::IndexBuf) fullDescription.bindFlags |= BindFlag::IndexBuffer;

		bufferResources[tag] = std::move(device->CreateResource(fullDescription, description.second));
	}

	for (const auto& [tag, description] : transientTextureResources)
	{
		bool renderTarget = false;  // Is the resource ever used as a render target.
		bool depthStencil = false;  // Is the resource ever used as a depth stencil.
		bool defaultUsage = false;  // Is the resource ever used in default usage.
		bool written = dependencies[tag].writingPasses.size();  // Is the resource ever written to.

		for (const auto& [passIndex, usage] : usages[tag].passUsage)
		{
			if (usage == RGUsage::RenderTarget || usage == RGUsage::BackBuffer) renderTarget = true;
			else if (usage == RGUsage::DepthStencil) depthStencil = true;
			else if (usage == RGUsage::Default) defaultUsage = true;
		}

		VGAssert(!(renderTarget && depthStencil), "Texture cannot have render target and depth stencil usage!");

		TextureDescription fullDescription{};
		fullDescription.updateRate = ResourceFrequency::Static;
		fullDescription.bindFlags = defaultUsage ? BindFlag::ShaderResource : 0;  // If we have default usage, we're going to need SRV binding.
		fullDescription.accessFlags = written ? AccessFlag::GPUWrite : 0;
		fullDescription.width = description.first.width;
		fullDescription.height = description.first.height;
		fullDescription.depth = description.first.depth;
		fullDescription.format = description.first.format;

		if (renderTarget)
		{
			fullDescription.bindFlags |= BindFlag::RenderTarget;
			if (written && defaultUsage) fullDescription.bindFlags |= BindFlag::UnorderedAccess;  // Normal render target writes use a special render target state, not UAV.
		}
		else if (depthStencil) fullDescription.bindFlags |= BindFlag::DepthStencil;
		else if (written) fullDescription.bindFlags |= BindFlag::UnorderedAccess;

		textureResources[tag] = std::move(device->CreateResource(fullDescription, description.second));
	}
}

D3D12_RESOURCE_STATES RGResolver::DetermineInitialState(size_t resourceTag)
{
	if (bufferResources.count(resourceTag))
	{
		return bufferResources[resourceTag]->state;
	}

	else if (textureResources.count(resourceTag))
	{
		return textureResources[resourceTag]->state;
	}

	VGEnsure(false, "Failed to determine initial resource state.");
	return {};
}