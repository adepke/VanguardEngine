// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraphResolver.h>
#include <Rendering/Device.h>

void RGResolver::BuildTransients(RenderDevice* Device, std::unordered_map<size_t, ResourceDependencyData>& Dependencies, std::unordered_map<size_t, ResourceUsageData>& Usages)
{
	for (const auto& [Tag, Description] : TransientBufferResources)
	{
		bool Written = Dependencies[Tag].WritingPasses.size();  // Is the resource ever written to.

		BufferDescription FullDescription;
		FullDescription.UpdateRate = Description.first.UpdateRate;
		FullDescription.BindFlags = BindFlag::ShaderResource;
		FullDescription.AccessFlags = Written ? AccessFlag::GPUWrite : 0;
		//FullDescription.InitialState = ;  // #TEMP
		FullDescription.Size = Description.first.Size;
		FullDescription.Stride = Description.first.Stride;
		FullDescription.Format = Description.first.Format;

		if (Description.first.BufferTypeFlags & RGBufferTypeFlag::ConstantBuf) FullDescription.BindFlags |= BindFlag::ConstantBuffer;
		if (Description.first.BufferTypeFlags & RGBufferTypeFlag::VertexBuf) FullDescription.BindFlags |= BindFlag::VertexBuffer;
		if (Description.first.BufferTypeFlags & RGBufferTypeFlag::IndexBuf) FullDescription.BindFlags |= BindFlag::IndexBuffer;

		BufferResources[Tag] = std::move(Device->CreateResource(FullDescription, Description.second));
	}

	for (const auto& [Tag, Description] : TransientTextureResources)
	{
		bool RenderTarget = false;  // Is the resource ever used as a render target.
		bool DepthStencil = false;  // Is the resource ever used as a depth stencil.
		bool Written = Dependencies[Tag].WritingPasses.size();  // Is the resource ever written to.

		for (const auto& [PassIndex, Usage] : Usages[Tag].PassUsage)
		{
			if (Usage == RGUsage::RenderTarget || Usage == RGUsage::BackBuffer) RenderTarget = true;
			else if (Usage == RGUsage::DepthStencil) DepthStencil = true;
		}

		VGAssert(!(RenderTarget && DepthStencil), "Texture cannot have render target and depth stencil usage!");

		TextureDescription FullDescription{};
		FullDescription.UpdateRate = ResourceFrequency::Static;
		FullDescription.BindFlags = BindFlag::ShaderResource;
		FullDescription.AccessFlags = Written ? AccessFlag::GPUWrite : 0;
		//FullDescription.InitialState = DepthStencil ? : ;  // #TEMP
		FullDescription.Width = Description.first.Width;
		FullDescription.Height = Description.first.Height;
		FullDescription.Depth = Description.first.Depth;
		FullDescription.Format = Description.first.Format;

		if (RenderTarget) FullDescription.BindFlags |= BindFlag::RenderTarget;
		else if (DepthStencil) FullDescription.BindFlags |= BindFlag::DepthStencil;
		if (Written) FullDescription.BindFlags |= BindFlag::UnorderedAccess;

		TextureResources[Tag] = std::move(Device->CreateResource(FullDescription, Description.second));
	}
}

D3D12_RESOURCE_STATES RGResolver::DetermineInitialState(size_t ResourceTag)
{
	if (BufferResources.count(ResourceTag))
	{
		return BufferResources[ResourceTag]->State;
	}

	else if (TextureResources.count(ResourceTag))
	{
		return TextureResources[ResourceTag]->State;
	}

	VGEnsure(false, "Failed to determine initial resource state.");
	return {};
}