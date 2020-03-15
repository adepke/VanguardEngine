// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraphResolver.h>

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <optional>

class RenderDevice;
struct Buffer;
struct Texture;

struct ResourceDependencyData
{
	std::unordered_set<size_t> ReadingPasses;  // List of passes that read from this resource.
	std::unordered_set<size_t> WritingPasses;  // List of passes that write to this resource.
};

struct ResourceUsageData
{
	std::unordered_map<size_t, RGUsage> PassUsage;  // Map of pass index to usage.
};

class RenderGraph
{
private:
	RenderDevice* Device;
	size_t TagCounter = 0;

	std::vector<std::unique_ptr<RenderPass>> Passes;
	std::vector<size_t> PassPipeline;
	std::unordered_map<size_t, ResourceDependencyData> ResourceDependencies;  // Maps resource tag to dependency data.
	std::unordered_map<size_t, ResourceUsageData> ResourceUsages;  // Maps resource tag to usage data.

	std::optional<size_t> FindUsage(RGUsage Usage);  // Searches for resources with the specified usage. If multiple exist, returns the first found.
	bool Validate();  // Ensures we have a valid graph.
	std::unordered_set<size_t> FindWrittenResources(size_t PassIndex);  // Searches for all resources that the specified pass writes to.
	void Traverse(size_t ResourceTag);  // Walk up the dependency chain, recursively adding passes to the stack. 
	void Serialize();  // Serializes to an optimized linear pipeline.

public:  // Utilities for render passes.
	inline void AddResourceRead(size_t PassIndex, size_t ResourceTag, RGUsage Usage);
	inline void AddResourceWrite(size_t PassIndex, size_t ResourceTag, RGUsage Usage);

public:
	RenderGraph(RenderDevice* InDevice) : Device(InDevice) {}

	size_t GetNextResourceTag() noexcept
	{
		return TagCounter++;
	}

	// Imports an externally-managed resource, such as the swap chain surface.
	size_t ImportResource(std::shared_ptr<Buffer>& Resource);
	size_t ImportResource(std::shared_ptr<Texture>& Resource);

	RenderPass& AddPass(const std::wstring& Name);

	void Build();
	void Execute();
};

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