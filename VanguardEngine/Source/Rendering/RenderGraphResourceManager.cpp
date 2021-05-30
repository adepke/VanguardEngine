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
		bool foundReusable = false;

		// Attempt to reuse an existing transient.
		for (auto& transientBuffer : transientBuffers)
		{
			if (transientBuffer.counter > 0 && info.first == transientBuffer.description)
			{
				foundReusable = true;
				--transientBuffer.counter;
				bufferResources[resource] = bufferResources[transientBuffer.resource];  // Duplicate the resource handle.
				break;
			}
		}

		if (!foundReusable)
		{
			// Fallback to creating a new buffer.
			VGLog(Rendering) << "Did not find a suitable buffer for transient reuse, creating a new buffer.";

			bool hasConstantBuffer = false;
			bool hasShaderResource = false;
			bool hasUnorderedAccess = false;

			for (const auto& pass : graph->passes)
			{
				if (pass->bindInfo.contains(resource))
				{
					switch (pass->bindInfo[resource])
					{
					case ResourceBind::CBV: hasConstantBuffer = true; break;
					case ResourceBind::SRV: hasShaderResource = true; break;
					case ResourceBind::UAV: hasUnorderedAccess = true; break;
					}
				}
			}

			BufferDescription description{};
			description.updateRate = info.first.updateRate;
			description.bindFlags = 0;
			description.accessFlags = AccessFlag::CPURead | AccessFlag::CPUWrite | AccessFlag::GPUWrite;
			description.size = info.first.size;
			description.stride = info.first.stride;
			description.format = info.first.format;

			if (hasConstantBuffer) description.bindFlags |= BindFlag::ConstantBuffer;
			if (hasShaderResource) description.bindFlags |= BindFlag::ShaderResource;
			if (hasUnorderedAccess) description.bindFlags |= BindFlag::UnorderedAccess;

			const auto buffer = device->GetResourceManager().Create(description, info.second);
			bufferResources[resource] = buffer;

			transientBuffers.emplace_front(resource, 0, info.first);
		}
	}

	transientBufferResources.clear();

	// Built all buffer textures, destroy unused transients and reset state.
	auto i = transientBuffers.begin();
	while (i != transientBuffers.end())
	{
		// If the transient wasn't reused, discard it.
		if (i->counter > 1)
		{
			device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), bufferResources[i->resource]);
			transientBuffers.erase(i++);
		}

		else
		{
			i->counter = 1;
			++i;
		}
	}

	const auto [outputWidth, outputHeight] = graph->GetBackBufferResolution(device);

	for (const auto& [resource, info] : transientTextureResources)
	{
		bool foundReusable = false;

		// Attempt to reuse an existing transient.
		for (auto& transientTexture : transientTextures)
		{
			if (transientTexture.counter > 0 && info.first == transientTexture.description)
			{
				foundReusable = true;
				--transientTexture.counter;
				textureResources[resource] = textureResources[transientTexture.resource];  // Duplicate the resource handle.
				break;
			}
		}

		if (!foundReusable)
		{
			// Fallback to creating a new texture.
			VGLog(Rendering) << "Did not find a suitable texture for transient reuse, creating a new texture.";

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

			TextureDescription description{};
			description.bindFlags = 0;  // Can't always assume SRV, depth stencils must be in a special state for that.
			description.accessFlags = AccessFlag::CPURead | AccessFlag::CPUWrite | AccessFlag::GPUWrite;
			description.width = info.first.width;
			description.height = info.first.height;
			description.depth = info.first.depth;
			description.format = info.first.format;

			if (hasShaderResource) description.bindFlags |= BindFlag::ShaderResource;
			if (hasRenderTarget) description.bindFlags |= BindFlag::RenderTarget;
			if (hasDepthStencil) description.bindFlags |= BindFlag::DepthStencil;

			if (description.width == 0 || description.height == 0)
			{
				description.width = outputWidth;
				description.height = outputHeight;
			}

			const auto texture = device->GetResourceManager().Create(description, info.second);
			textureResources[resource] = texture;

			transientTextures.emplace_front(resource, 0, info.first);
		}
	}

	transientTextureResources.clear();

	// Built all transient textures, destroy unused transients and reset state.
	auto j = transientTextures.begin();
	while (j != transientTextures.end())
	{
		// If the transient wasn't reused, discard it.
		if (j->counter > 1)
		{
			device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), textureResources[j->resource]);
			transientTextures.erase(j++);
		}

		else
		{
			j->counter = 1;
			++j;
		}
	}
}

void RenderGraphResourceManager::DiscardTransients(RenderDevice* device)
{
	for (const auto& transient : transientBuffers)
	{
		device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), bufferResources[transient.resource]);
	}

	transientBuffers.clear();

	for (const auto& transient : transientTextures)
	{
		device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), textureResources[transient.resource]);
	}

	transientTextures.clear();
}