// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/RenderGraphResourceManager.h>

#include <vector>
#include <memory>
#include <unordered_map>
#include <stack>
#include <string_view>

class RenderPass;
class CommandList;

enum class ExecutionQueue
{
	Graphics,
	Compute
};

enum class ResourceTag
{
	BackBuffer
};

class RenderGraph
{
	friend class RenderGraphResourceManager;

private:
	std::vector<std::unique_ptr<RenderPass>> passes;
	std::vector<std::shared_ptr<CommandList>> passLists;
	std::unordered_map<size_t, std::vector<size_t>> adjacencyList;
	std::vector<size_t> sorted;
	std::unordered_map<size_t, std::uint32_t> depthMap;

	std::unordered_map<ResourceTag, RenderResource> taggedResources;

	RenderGraphResourceManager resourceManager;

private:
	void BuildAdjacencyList();
	void DepthFirstSearch(size_t node, std::vector<bool>& visited, std::stack<bool>& stack);
	void TopologicalSort();
	void BuildDepthMap();

	void InjectStateBarriers();

public:
	std::pair<uint32_t, uint32_t> GetBackBufferResolution();

public:
	const RenderResource Import(const std::shared_ptr<Buffer>& resource);
	const RenderResource Import(const std::shared_ptr<Texture>& resource);

	void Tag(const RenderResource resource, ResourceTag tag);

	RenderPass& AddPass(std::string_view stableName, ExecutionQueue execution);

	void Build();
	void Execute(RenderDevice* device);
};

inline const RenderResource RenderGraph::Import(const std::shared_ptr<Buffer>& resource)
{
	return resourceManager.AddResource(resource);
}

inline const RenderResource RenderGraph::Import(const std::shared_ptr<Texture>& resource)
{
	return resourceManager.AddResource(resource);
}

inline void RenderGraph::Tag(const RenderResource resource, ResourceTag tag)
{
	taggedResources[tag] = resource;
}