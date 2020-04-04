// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraphResolver.h>

void RGResolver::BuildTransients(RenderDevice* Device, const std::unordered_map<size_t, ResourceDependencyData>& Dependencies, const std::unordered_map<size_t, ResourceUsageData>& Usages)
{
	for (const auto& [Tag, Description] : TransientBufferResources)
	{
		bool Written = Dependencies[Tag].WritingPasses.size();  // Is the resource ever written to.

		BufferDescription FullDescription;
		FullDescription.UpdateRate = Description.UpdateRate;
		FullDescription.BindFlags = ;
		FullDescription.AccessFlags = Written ? AccessFlag::GPUWrite : 0;
		FullDescription.InitialState = ;
		FullDescription.Size = Description.Size;
		FullDescription.Stride = Description.Stride;
		FullDescription.Format = Description.Format;

		BufferResources[Tag] = std::move(Device->CreateResource(FullDescription, ));
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
		FullDescription.InitialState = DepthStencil ? : ;
		FullDescription.Width = Description.Width;
		FullDescription.Height = Description.Height;
		FullDescription.Depth = Description.Depth;
		FullDescription.Format = Description.Format;

		if (RenderTarget) FullDescription.BindFlags |= BindFlag::RenderTarget;
		else if (DepthStencil) FullDescription.BindFlags |= BindFlag::DepthStencil;
		if (Written) FullDescription.BindFlags |= BindFlag::UnorderedAccess;

		TextureResources[Tag] = std::move(Device->CreateResource(FullDescription, ));
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