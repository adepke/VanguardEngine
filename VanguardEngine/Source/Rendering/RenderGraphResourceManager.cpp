// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/Device.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>

DescriptorHandle RenderGraphResourceManager::CreateDescriptorFromView(const RenderResource resource, ShaderResourceViewDescription viewDesc)
{
	VGScopedCPUStat("Create Descriptor From View");

	DescriptorHandle handle;
	switch (viewDesc.heap)
	{
	case HeapType::Visible:
		handle = device->AllocateDescriptor(DescriptorType::Default);
		break;
	case HeapType::NonVisible:
		handle = device->GetDescriptorAllocator().AllocateNonVisible();
		break;
	}

	ID3D12Resource* allocation = nullptr;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};

	// #TODO: Reduce duplicated descriptor code from ResourceManager.

	if (auto buffer = GetOptionalBuffer(resource); buffer)
	{
		auto& component = device->GetResourceManager().Get(*buffer);
		allocation = component.Native();
		const auto& desc = std::get<ShaderResourceViewDescription::BufferDesc>(viewDesc.data);

		srvDesc.Format = component.description.format ? *component.description.format : DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = desc.start;
		srvDesc.Buffer.NumElements = desc.count > 0 ? desc.count : static_cast<UINT>(component.description.size);
		srvDesc.Buffer.StructureByteStride = !component.description.format || *component.description.format == DXGI_FORMAT_UNKNOWN ? static_cast<UINT>(component.description.stride) : 0;
		srvDesc.Buffer.Flags = component.description.format == DXGI_FORMAT_R32_TYPELESS ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

		uavDesc.Format = srvDesc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = srvDesc.Buffer.FirstElement;
		uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements;
		uavDesc.Buffer.StructureByteStride = srvDesc.Buffer.StructureByteStride;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = component.description.format == DXGI_FORMAT_R32_TYPELESS ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;
	}

	else if (auto texture = GetOptionalTexture(resource); texture)
	{
		auto& component = device->GetResourceManager().Get(*texture);
		allocation = component.Native();
		const auto& desc = std::get<ShaderResourceViewDescription::TextureDesc>(viewDesc.data);

		srvDesc.Format = component.description.format;

		// Using a depth stencil via SRV requires special formatting.
		if (component.description.bindFlags & BindFlag::DepthStencil)
		{
			switch (srvDesc.Format)
			{
			case DXGI_FORMAT_R32_TYPELESS: srvDesc.Format = DXGI_FORMAT_R32_FLOAT; break;
			case DXGI_FORMAT_R24G8_TYPELESS: srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; break;
			}
		}

		switch (component.Native()->GetDesc().Dimension)  // #TODO: Support texture arrays and multi-sample textures.
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			srvDesc.Texture1D.MostDetailedMip = desc.firstMip;
			srvDesc.Texture1D.MipLevels = desc.mipLevels;
			srvDesc.Texture1D.ResourceMinLODClamp = 0.f;

			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			uavDesc.Texture1D.MipSlice = desc.mip;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			if (component.description.depth == 1)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MostDetailedMip = desc.firstMip;
				srvDesc.Texture2D.MipLevels = desc.mipLevels;
				srvDesc.Texture2D.PlaneSlice = 0;
				srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = desc.mip;
				uavDesc.Texture2D.PlaneSlice = srvDesc.Texture2D.PlaneSlice;
				break;
			}

			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Texture2DArray.MostDetailedMip = desc.firstMip;
				srvDesc.Texture2DArray.MipLevels = desc.mipLevels;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.ArraySize = component.description.depth;
				srvDesc.Texture2DArray.PlaneSlice = 0;
				srvDesc.Texture2DArray.ResourceMinLODClamp = 0.f;

				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.MipSlice = desc.mip;
				uavDesc.Texture2DArray.FirstArraySlice = srvDesc.Texture2DArray.FirstArraySlice;
				uavDesc.Texture2DArray.ArraySize = srvDesc.Texture2DArray.ArraySize;
				uavDesc.Texture2DArray.PlaneSlice = srvDesc.Texture2DArray.PlaneSlice;
				break;
			}
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srvDesc.Texture3D.MostDetailedMip = desc.firstMip;
			srvDesc.Texture3D.MipLevels = desc.mipLevels;
			srvDesc.Texture3D.ResourceMinLODClamp = 0.f;

			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			uavDesc.Texture3D.MipSlice = desc.mip;
			uavDesc.Texture3D.FirstWSlice = 0;
			uavDesc.Texture3D.WSize = -1;
			break;
		}
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		uavDesc.Format = srvDesc.Format;
	}

	switch (viewDesc.bind)
	{
	case ResourceBind::SRV:
		device->Native()->CreateShaderResourceView(allocation, &srvDesc, handle);
		break;

	case ResourceBind::UAV:
		device->Native()->CreateUnorderedAccessView(allocation, nullptr, &uavDesc, handle);
		break;
	}

	return std::move(handle);
}

uint32_t RenderGraphResourceManager::GetDefaultDescriptor(const RenderResource resource, ResourceBind bind)
{
	switch (bind)
	{
	case ResourceBind::SRV:
		if (auto buffer = GetOptionalBuffer(resource); buffer)
		{
			return device->GetResourceManager().Get(*buffer).SRV->bindlessIndex;
		}

		else if (auto texture = GetOptionalTexture(resource); texture)
		{
			return device->GetResourceManager().Get(*texture).SRV->bindlessIndex;
		}
		break;
	case ResourceBind::UAV:
		if (auto buffer = GetOptionalBuffer(resource); buffer)
		{
			return device->GetResourceManager().Get(*buffer).UAV->bindlessIndex;
		}

		else if (auto texture = GetOptionalTexture(resource); texture)
		{
			// #TEMP
			//VGEnsure(false, "Textures do not have a default UAV descriptor created.");
			return -1;
		}
		break;
	}

	return -1;
}

void RenderGraphResourceManager::SearchCrossFrameTransients(RenderGraph* graph)
{
	// We can't check if there's an intersection between pass->reads or pass->writes and the transient resource,
	// since we can have multiple different resource handles pointing to the same underlying resource.

	for (auto& transientBuffer : transientBuffers)
	{
		for (const auto& pass : graph->passes)
		{
			std::vector<entt::entity> accessedHandles;
			accessedHandles.reserve(pass->reads.size() + pass->writes.size());

			for (const auto read : pass->reads)
			{
				if (bufferResources.contains(read))
					accessedHandles.emplace_back(bufferResources[read].handle);
			}
			for (const auto write : pass->writes)
			{
				if (bufferResources.contains(write))
					accessedHandles.emplace_back(bufferResources[write].handle);
			}

			const auto handle = bufferResources[transientBuffer.resource].handle;
			if (std::find(accessedHandles.begin(), accessedHandles.end(), handle) != accessedHandles.end())
			{
				transientBuffer.counter = 0;
				break;
			}
		}
	}

	for (auto& transientTexture : transientTextures)
	{
		for (const auto& pass : graph->passes)
		{
			std::vector<entt::entity> accessedHandles;
			accessedHandles.reserve(pass->reads.size() + pass->writes.size());

			for (const auto read : pass->reads)
			{
				if (textureResources.contains(read))
					accessedHandles.emplace_back(textureResources[read].handle);
			}
			for (const auto write : pass->writes)
			{
				if (textureResources.contains(write))
					accessedHandles.emplace_back(textureResources[write].handle);
			}

			const auto handle = textureResources[transientTexture.resource].handle;
			if (std::find(accessedHandles.begin(), accessedHandles.end(), handle) != accessedHandles.end())
			{
				transientTexture.counter = 0;
				break;
			}
		}
	}
}

void RenderGraphResourceManager::BuildTransients(RenderGraph* graph)
{
	VGScopedCPUStat("Render Graph Build Transients");
	VGScopedGPUStat("Render Graph Build Transients", device->GetDirectContext(), device->GetDirectList().Native());

	// Transients used across frames need special handling.
	SearchCrossFrameTransients(graph);

	// #TODO: Don't brute force search all passes to determine bind flags. Use a better approach.

	for (const auto& [resource, info] : transientBufferResources)
	{
		bool foundReusable = false;

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

		// UAV counter implies UAV.
		if (info.first.uavCounter)
			hasUnorderedAccess = true;

		if (transientReuse)
		{
			// Attempt to reuse an existing transient.
			for (auto& transientBuffer : transientBuffers)
			{
				if (transientBuffer.counter > 0 && info.first == transientBuffer.description)
				{
					// Verify the bind flags at least cover all the states we need in this pass.
					if ((hasConstantBuffer && !(transientBuffer.binds & BindFlag::ConstantBuffer)) ||
						(hasShaderResource && !(transientBuffer.binds & BindFlag::ShaderResource)) ||
						(hasUnorderedAccess && !(transientBuffer.binds & BindFlag::UnorderedAccess)))
					{
						continue;
					}

					foundReusable = true;
					transientBuffer.counter = 0;
					bufferResources[resource] = bufferResources[transientBuffer.resource];  // Duplicate the resource handle.

					// If we have a UAV counter, we need to reset it.
					if (transientBuffer.description.uavCounter)
					{
						auto& bufferComponent = device->GetResourceManager().Get(bufferResources[resource]);
						device->GetResourceManager().Write(bufferComponent.counterBuffer, { 0, 0, 0, 0 });  // #TODO: Use CopyBufferRegion with a clear buffer created once at startup.
					}

					device->GetResourceManager().NameResource(bufferResources[resource], info.second);

					break;
				}
			}
		}

		if (!foundReusable)
		{
			// Fallback to creating a new buffer.
			VGLog(logRendering, "Did not find a suitable buffer for transient reuse, creating a new buffer for '{}'.", info.second);

			BufferDescription description{};
			description.updateRate = info.first.updateRate;
			description.bindFlags = 0;
			description.accessFlags = AccessFlag::CPURead | AccessFlag::CPUWrite | AccessFlag::GPUWrite;
			description.size = info.first.size;
			description.stride = info.first.stride;
			description.uavCounter = info.first.uavCounter;
			description.format = info.first.format;

			if (hasConstantBuffer) description.bindFlags |= BindFlag::ConstantBuffer;
			if (hasShaderResource) description.bindFlags |= BindFlag::ShaderResource;
			// Some passes use SRV's of resources in UAV states, like mipmap generation.
			if (hasUnorderedAccess) description.bindFlags |= BindFlag::UnorderedAccess | BindFlag::ShaderResource;

			const auto buffer = device->GetResourceManager().Create(description, info.second);
			bufferResources[resource] = buffer;

			transientBuffers.emplace_front(resource, 0, description.bindFlags, info.first);
		}
	}

	transientBufferResources.clear();

	// Built all transient buffers, destroy unused transients and reset state.
	auto i = transientBuffers.begin();
	while (i != transientBuffers.end())
	{
		// If the transient wasn't reused recently, discard it.
		if (i->counter > transientExpiration)
		{
			device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), bufferResources[i->resource]);
			i = transientBuffers.erase(i);
		}

		else
		{
			i->counter++;
			++i;
		}
	}

	const auto [outputWidth, outputHeight] = graph->GetBackBufferResolution(device);

	for (const auto& [resource, info] : transientTextureResources)
	{
		bool foundReusable = false;

		// We can't have both depth stencil and render target, so brute force search all passes to determine
		// which binding we need.

		bool hasShaderResource = false;
		bool hasUnorderedAccess = false;
		bool hasRenderTarget = false;
		bool hasDepthStencil = false;

		for (const auto& pass : graph->passes)
		{
			if (pass->bindInfo.contains(resource))
			{
				switch (pass->bindInfo[resource])
				{
				case ResourceBind::SRV: hasShaderResource = true; break;
				case ResourceBind::UAV: hasUnorderedAccess = true; break;
				case ResourceBind::DSV: hasDepthStencil = true; break;
				}
			}

			if (pass->outputBindInfo.contains(resource))
			{
				const auto [bind, clear] = pass->outputBindInfo[resource];

				if (bind == OutputBind::RTV) hasRenderTarget = true;
				else if (bind == OutputBind::DSV) hasDepthStencil = true;
			}
		}

		VGAssert(!(hasRenderTarget && hasDepthStencil), "Texture cannot have render target and depth stencil bindings!");

		if (transientReuse)
		{
			// Attempt to reuse an existing transient.
			for (auto& transientTexture : transientTextures)
			{
				if (transientTexture.counter > 0 && info.first == transientTexture.description)
				{
					// Verify the bind flags at least cover all the states we need in this pass.
					if ((hasShaderResource && !(transientTexture.binds & BindFlag::ShaderResource)) ||
						(hasUnorderedAccess && !(transientTexture.binds & BindFlag::UnorderedAccess)) ||
						(hasRenderTarget && !(transientTexture.binds & BindFlag::RenderTarget)) ||
						(hasDepthStencil && !(transientTexture.binds & BindFlag::DepthStencil)))
					{
						continue;
					}

					foundReusable = true;
					transientTexture.counter = 0;
					textureResources[resource] = textureResources[transientTexture.resource];  // Duplicate the resource handle.

					device->GetResourceManager().NameResource(textureResources[resource], info.second);

					break;
				}
			}
		}

		if (!foundReusable)
		{
			// Fallback to creating a new texture.
			VGLog(logRendering, "Did not find a suitable texture for transient reuse, creating a new texture for '{}'.", info.second);

			TextureDescription description{};
			description.bindFlags = 0;  // Can't always assume SRV, depth stencils must be in a special state for that.
			description.accessFlags = AccessFlag::CPURead | AccessFlag::CPUWrite | AccessFlag::GPUWrite;
			description.width = info.first.width;
			description.height = info.first.height;
			description.depth = info.first.depth;
			description.format = info.first.format;
			description.mipMapping = info.first.mipMapping;

			if (hasShaderResource) description.bindFlags |= BindFlag::ShaderResource;
			// Some passes use SRV's of resources in UAV states, like mipmap generation.
			if (hasUnorderedAccess) description.bindFlags |= BindFlag::UnorderedAccess | BindFlag::ShaderResource;
			if (hasRenderTarget) description.bindFlags |= BindFlag::RenderTarget;
			if (hasDepthStencil) description.bindFlags |= BindFlag::DepthStencil;

			if (description.width == 0 || description.height == 0)
			{
				description.width = outputWidth * info.first.resolutionScale;
				description.height = outputHeight * info.first.resolutionScale;
			}

			const auto texture = device->GetResourceManager().Create(description, info.second);
			textureResources[resource] = texture;

			transientTextures.emplace_front(resource, 0, description.bindFlags, info.first);
		}
	}

	transientTextureResources.clear();

	// Built all transient textures, destroy unused transients and reset state.
	auto j = transientTextures.begin();
	while (j != transientTextures.end())
	{
		// If the transient wasn't reused recently, discard it.
		if (j->counter > transientExpiration)
		{
			device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), textureResources[j->resource]);
			j = transientTextures.erase(j);
		}

		else
		{
			j->counter++;
			++j;
		}
	}
}

void RenderGraphResourceManager::BuildDescriptors(RenderGraph* graph)
{
	VGScopedCPUStat("Render Graph Build Descriptors");

	for (int i = 0; i < graph->passes.size(); ++i)
	{
		const auto& pass = graph->passes[i];

		for (const auto& [resource, requests] : pass->descriptorInfo)
		{
			// Check if we want a default descriptor or a custom set.
			if (requests.descriptorRequests.size() == 0)
			{
				passViews[i].views[resource].descriptorIndices[""] = GetDefaultDescriptor(resource, pass->bindInfo[resource]);
			}

			else
			{
				for (const auto& [name, request] : requests.descriptorRequests)
				{
					// #TODO: Don't recreate descriptors every frame.
					auto descriptor = CreateDescriptorFromView(resource, request);
					passViews[i].views[resource].descriptorIndices[name] = descriptor.bindlessIndex;
					passViews[i].views[resource].fullDescriptors[name] = std::move(descriptor);
				}
			}
		}
	}
}
void RenderGraphResourceManager::DiscardTransients()
{
	VGScopedCPUStat("Render Graph Discard Transients");

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

void RenderGraphResourceManager::DiscardDescriptors()
{
	VGScopedCPUStat("Render Graph Discard Descriptors");

	for (auto& [pass, views] : passViews)
	{
		for (auto& [resource, view] : views.views)
		{
			for (auto it = view.fullDescriptors.begin(); it != view.fullDescriptors.end();)
			{
				device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(it->second));
				it = view.fullDescriptors.erase(it);
			}
		}
	}

	passViews.clear();
}

void RenderGraphResourceManager::DiscardPipelines()
{
	passPipelines.clear();
}