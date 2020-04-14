// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderGraphResource.h>

size_t RenderPass::CreateResource(const RGBufferDescription& Description, const std::wstring& Name)
{
	return Graph.AddTransientResource(Description, Name);
}

size_t RenderPass::CreateResource(const RGTextureDescription& Description, const std::wstring& Name)
{
	return Graph.AddTransientResource(Description, Name);
}

void RenderPass::ReadResource(size_t ResourceTag, RGUsage Usage)
{
	Graph.AddResourceRead(Index, ResourceTag, Usage);
}

void RenderPass::WriteResource(size_t ResourceTag, RGUsage Usage)
{
	Graph.AddResourceWrite(Index, ResourceTag, Usage);
}