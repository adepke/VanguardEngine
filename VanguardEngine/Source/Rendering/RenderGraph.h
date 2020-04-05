// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/RenderGraphResolver.h>

#include <vector>
#include <memory>

class RenderDevice;
struct Buffer;
struct Texture;

class RenderGraph
{
private:
	RenderDevice* Device;
	RGResolver Resolver;

	std::vector<std::unique_ptr<RenderPass>> Passes;
	std::vector<size_t> PassPipeline;
	std::unordered_map<size_t, ResourceDependencyData> ResourceDependencies;  // Maps resource tag to dependency data.
	std::unordered_map<size_t, ResourceUsageData> ResourceUsages;  // Maps resource tag to usage data.

	std::optional<size_t> FindUsage(RGUsage Usage);  // Searches for resources with the specified usage. If multiple exist, returns the first found.
	bool Validate();  // Ensures we have a valid graph.
	std::unordered_set<size_t> FindReadResources(size_t PassIndex);  // Searches for all resources that the specified pass reads from.
	std::unordered_set<size_t> FindWrittenResources(size_t PassIndex);  // Searches for all resources that the specified pass writes to.
	void Traverse(size_t ResourceTag);  // Walk up the dependency chain, recursively adding passes to the stack. 
	void Serialize();  // Serializes to an optimized linear pipeline.
	void InjectBarriers(std::vector<CommandList>& Lists);  // Places barriers in a serialized pipeline for usage changes on resources.

public:  // Utilities for render passes.
	inline size_t AddTransientResource(const RGBufferDescription& Description, const std::wstring& Name);
	inline size_t AddTransientResource(const RGTextureDescription& Description, const std::wstring& Name);
	inline void AddResourceRead(size_t PassIndex, size_t ResourceTag, RGUsage Usage);
	inline void AddResourceWrite(size_t PassIndex, size_t ResourceTag, RGUsage Usage);

public:
	RenderGraph(RenderDevice* InDevice) : Device(InDevice) {}

	// Imports an externally-managed resource, such as the swap chain surface.
	size_t ImportResource(std::shared_ptr<Buffer>& Resource) { return Resolver.AddResource(Resource); }
	size_t ImportResource(std::shared_ptr<Texture>& Resource) { return Resolver.AddResource(Resource); }

	RenderPass& AddPass(const std::wstring& Name);

	void Build();
	void Execute();
};

inline size_t RenderGraph::AddTransientResource(const RGBufferDescription& Description, const std::wstring& Name)
{
	const auto Result = Resolver.AddTransientResource(Description);
	Resolver.NameTransientBuffer(Result, Name);

	return Result;
}

inline size_t RenderGraph::AddTransientResource(const RGTextureDescription& Description, const std::wstring& Name)
{
	const auto Result = Resolver.AddTransientResource(Description);
	Resolver.NameTransientTexture(Result, Name);

	return Result;
}

inline void RenderGraph::AddResourceRead(size_t PassIndex, size_t ResourceTag, RGUsage Usage)
{
	ResourceDependencies[ResourceTag].ReadingPasses.insert(PassIndex);
	ResourceUsages[ResourceTag].PassUsage[PassIndex] = Usage;
}

inline void RenderGraph::AddResourceWrite(size_t PassIndex, size_t ResourceTag, RGUsage Usage)
{
	ResourceDependencies[ResourceTag].WritingPasses.insert(PassIndex);
	ResourceUsages[ResourceTag].PassUsage[PassIndex] = Usage;
}