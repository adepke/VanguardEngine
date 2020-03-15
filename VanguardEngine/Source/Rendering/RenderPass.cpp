// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderGraphResource.h>

void RenderPass::ReadResource(size_t ResourceTag, RGUsage Usage)
{
	Graph.AddResourceRead(Index, ResourceTag, Usage);
}

void RenderPass::WriteResource(size_t ResourceTag, RGUsage Usage)
{
	Graph.AddResourceWrite(Index, ResourceTag, Usage);
}