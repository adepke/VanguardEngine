// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraphResourceManager.h>

#include <vector>
#include <memory>
#include <unordered_map>
#include <stack>
#include <string_view>

class CommandList;
class Renderer;
class RenderPipelineLayout;
class PipelineState;

enum class ResourceTag
{
	BackBuffer
};

class RenderGraph
{
	friend class RenderGraphResourceManager;
	friend class Renderer;

private:
	std::vector<std::unique_ptr<RenderPass>> passes;
	std::vector<std::shared_ptr<CommandList>> passLists;
	std::unordered_map<size_t, std::vector<size_t>> adjacencyLists;
	std::vector<size_t> sorted;
	std::unordered_map<size_t, std::uint32_t> depthMap;

	std::unordered_map<ResourceTag, RenderResource> taggedResources;

	RenderGraphResourceManager* resourceManager = nullptr;

private:
	void BuildAdjacencyLists();
	void DepthFirstSearch(size_t node, std::vector<bool>& visited, std::stack<size_t>& stack);
	void TopologicalSort();
	void BuildDepthMap();

	void InjectBarriers(RenderDevice* device, size_t passId);

public:
	std::pair<uint32_t, uint32_t> GetBackBufferResolution(RenderDevice* device);

public:
	RenderGraph(RenderGraphResourceManager* resources) : resourceManager(resources) {};

	const RenderResource Import(const BufferHandle resource);
	const RenderResource Import(const TextureHandle resource);
	void Tag(const RenderResource resource, ResourceTag tag);
	PipelineState& RequestPipelineState(RenderDevice* device, const RenderPipelineLayout& layout, size_t passIndex);
	RenderPass& AddPass(std::string_view stableName, ExecutionQueue execution);
	void Build();
	void Execute(RenderDevice* device);
};

inline const RenderResource RenderGraph::Import(const BufferHandle resource)
{
	return resourceManager->AddResource(resource);
}

inline const RenderResource RenderGraph::Import(const TextureHandle resource)
{
	return resourceManager->AddResource(resource);
}

inline void RenderGraph::Tag(const RenderResource resource, ResourceTag tag)
{
	taggedResources[tag] = resource;
}