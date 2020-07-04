// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderGraphResource.h>

size_t RenderPass::CreateResource(const RGBufferDescription& description, const std::wstring& name)
{
	return graph.AddTransientResource(description, name);
}

size_t RenderPass::CreateResource(const RGTextureDescription& description, const std::wstring& name)
{
	return graph.AddTransientResource(description, name);
}

void RenderPass::ReadResource(size_t resourceTag, RGUsage usage)
{
	graph.AddResourceRead(index, resourceTag, usage);
}

void RenderPass::WriteResource(size_t resourceTag, RGUsage usage)
{
	graph.AddResourceWrite(index, resourceTag, usage);
}