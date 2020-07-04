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
	RenderDevice* device;
	RGResolver resolver;

	std::vector<std::unique_ptr<RenderPass>> passes;
	std::vector<size_t> passPipeline;
	std::unordered_map<size_t, ResourceDependencyData> resourceDependencies;  // Maps resource tag to dependency data.
	std::unordered_map<size_t, ResourceUsageData> resourceUsages;  // Maps resource tag to usage data.

	std::optional<size_t> FindUsage(RGUsage usage);  // Searches for resources with the specified usage. If multiple exist, returns the first found.
	bool Validate();  // Ensures we have a valid graph.
	std::unordered_set<size_t> FindReadResources(size_t passIndex);  // Searches for all resources that the specified pass reads from.
	std::unordered_set<size_t> FindWrittenResources(size_t passIndex);  // Searches for all resources that the specified pass writes to.
	void Traverse(std::unordered_set<size_t>& trackedPasses, size_t resourceTag);  // Walk up the dependency chain, recursively adding passes to the stack. 
	void Serialize();  // Serializes to an optimized linear pipeline.
	void InjectStateBarriers(std::vector<std::shared_ptr<CommandList>>& lists);  // Places barriers in a serialized pipeline for usage changes on resources.
	void InjectRaceBarriers(std::vector<std::shared_ptr<CommandList>>& lists);  // Places barriers to prevent against data hazards through race conditions.

public:  // Utilities for render passes.
	inline size_t AddTransientResource(const RGBufferDescription& description, const std::wstring& name);
	inline size_t AddTransientResource(const RGTextureDescription& description, const std::wstring& name);
	inline void AddResourceRead(size_t passIndex, size_t resourceTag, RGUsage usage);
	inline void AddResourceWrite(size_t passIndex, size_t resourceTag, RGUsage usage);

public:
	RenderGraph(RenderDevice* inDevice) : device(inDevice) {}

	// Imports an externally-managed resource, such as the swap chain surface.
	size_t ImportResource(std::shared_ptr<Buffer>& resource) { return resolver.AddResource(resource); }
	size_t ImportResource(std::shared_ptr<Texture>& resource) { return resolver.AddResource(resource); }

	RenderPass& AddPass(const std::string_view staticName);

	void Build();
	void Execute();
};

inline size_t RenderGraph::AddTransientResource(const RGBufferDescription& description, const std::wstring& name)
{
	const auto result = resolver.AddTransientResource(description);
	resolver.NameTransientBuffer(result, name);

	return result;
}

inline size_t RenderGraph::AddTransientResource(const RGTextureDescription& description, const std::wstring& name)
{
	const auto result = resolver.AddTransientResource(description);
	resolver.NameTransientTexture(result, name);

	return result;
}

inline void RenderGraph::AddResourceRead(size_t passIndex, size_t resourceTag, RGUsage usage)
{
	resourceDependencies[resourceTag].readingPasses.insert(passIndex);
	resourceUsages[resourceTag].passUsage[passIndex] = usage;
}

inline void RenderGraph::AddResourceWrite(size_t passIndex, size_t resourceTag, RGUsage usage)
{
	resourceDependencies[resourceTag].writingPasses.insert(passIndex);
	resourceUsages[resourceTag].passUsage[passIndex] = usage;
}