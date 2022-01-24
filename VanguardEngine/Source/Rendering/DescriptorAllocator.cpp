// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/DescriptorAllocator.h>
#include <Rendering/Device.h>

void DescriptorAllocator::Initialize(RenderDevice* inDevice, size_t shaderDescriptors, size_t renderTargetDescriptors, size_t depthStencilDescriptors)
{
	VGScopedCPUStat("Descriptor Allocator Initialize");

	defaultHeap.Create(inDevice, DescriptorType::Default, shaderDescriptors, true);
	defaultNonVisibleHeap.Create(inDevice, DescriptorType::Default, shaderDescriptors, false);
	renderTargetHeap.Create(inDevice, DescriptorType::RenderTarget, renderTargetDescriptors, false);
	depthStencilHeap.Create(inDevice, DescriptorType::DepthStencil, depthStencilDescriptors, false);
}

DescriptorHandle DescriptorAllocator::Allocate(DescriptorType type)
{
	switch (type)
	{
	case DescriptorType::Default:
	case DescriptorType::Sampler:
		return defaultHeap.Allocate();
	case DescriptorType::RenderTarget:
		return renderTargetHeap.Allocate();
	case DescriptorType::DepthStencil:
		return depthStencilHeap.Allocate();
	}

	return {};
}

DescriptorHandle DescriptorAllocator::AllocateNonVisible()
{
	return defaultNonVisibleHeap.Allocate();
}

void DescriptorAllocator::FrameStep(size_t frameIndex)
{
	VGScopedCPUStat("Descriptor Allocator Frame Step");
}