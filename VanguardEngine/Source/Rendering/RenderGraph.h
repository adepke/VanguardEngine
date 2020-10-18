// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <vector>
#include <memory>
#include <stack>
#include <string_view>

enum class ExecutionQueue
{
	Graphics,
	Compute
};

class RenderGraph
{
private:
	std::vector<std::unique_ptr<RenderPass>> passes;
	std::unordered_map<size_t, std::vector<size_t>> adjacencyList;
	std::vector<size_t> sorted;
	std::unordered_map<size_t, std::uint32_t> depthMap;

private:
	void BuildAdjacencyList();
	void DepthFirstSearch(size_t node, std::vector<bool>& visited, std::stack<bool>& stack);
	void TopologicalSort();
	void BuildDepthMap();

public:
	const size_t Import(const std::shared_ptr<Buffer>& resource);
	const size_t Import(const std::shared_ptr<Texture>& resource);

	RenderPass& AddPass(const std::string_view stableName, ExecutionQueue executionQueue);

	void Build();
	void Execute();
};