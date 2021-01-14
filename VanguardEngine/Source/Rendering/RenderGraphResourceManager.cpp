// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/Device.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>

void RenderGraphResourceManager::BuildTransients(RenderDevice* device, RenderGraph* graph)
{
	VGScopedCPUStat("Render Graph Build Transients");

	// #TODO: For now just create resources in the most generic state. This should be refined to create a resource with a minimal state.

	for (const auto& [resource, info] : transientBufferResources)
	{
		BufferDescription fullDescription{};
		fullDescription.updateRate = info.first.updateRate;
		fullDescription.bindFlags = BindFlag::ConstantBuffer | BindFlag::ShaderResource | BindFlag::UnorderedAccess;
		fullDescription.accessFlags = AccessFlag::CPURead | AccessFlag::CPUWrite | AccessFlag::GPUWrite;
		fullDescription.size = info.first.size;
		fullDescription.stride = info.first.stride;
		fullDescription.format = info.first.format;

		const auto buffer = device->GetResourceManager().Create(fullDescription, info.second);
		bufferResources[resource] = buffer;
		device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), buffer);
	}

	const auto [outputWidth, outputHeight] = graph->GetBackBufferResolution(device);

	for (const auto& [resource, info] : transientTextureResources)
	{
		// We can't have both depth stencil and render target, so brute force search all passes to determine
		// which binding we need.

		bool hasShaderResource = false;
		bool hasRenderTarget = false;
		bool hasDepthStencil = false;

		for (const auto& pass : graph->passes)
		{
			if (pass->bindInfo.contains(resource))
			{
				if (pass->bindInfo[resource] == ResourceBind::SRV) hasShaderResource = true;
				else if (pass->bindInfo[resource] == ResourceBind::DSV) hasDepthStencil = true;
			}

			if (pass->outputBindInfo.contains(resource))
			{
				const auto [bind, clear] = pass->outputBindInfo[resource];

				if (bind == OutputBind::RTV) hasRenderTarget = true;
				else if (bind == OutputBind::DSV) hasDepthStencil = true;
			}
		}

		VGAssert(!(hasRenderTarget && hasDepthStencil), "Texture cannot have render target and depth stencil bindings!");

		TextureDescription fullDescription{};
		fullDescription.bindFlags = 0;  // Can't always assume SRV, depth stencils must be in a special state for that.
		fullDescription.accessFlags = AccessFlag::CPURead | AccessFlag::CPUWrite | AccessFlag::GPUWrite;
		fullDescription.width = info.first.width;
		fullDescription.height = info.first.height;
		fullDescription.depth = info.first.depth;
		fullDescription.format = info.first.format;

		if (hasShaderResource) fullDescription.bindFlags |= BindFlag::ShaderResource;
		if (hasRenderTarget) fullDescription.bindFlags |= BindFlag::RenderTarget;
		if (hasDepthStencil) fullDescription.bindFlags |= BindFlag::DepthStencil;

		if (fullDescription.width == 0 || fullDescription.height == 0)
		{
			fullDescription.width = outputWidth;
			fullDescription.height = outputHeight;
		}

		const auto texture = device->GetResourceManager().Create(fullDescription, info.second);
		textureResources[resource] = texture;
		device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), texture);
	}
}