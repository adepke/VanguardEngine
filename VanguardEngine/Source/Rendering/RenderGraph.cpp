// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraph.h>

#include <algorithm>

void RenderGraph::BuildAdjacencyList()
{
	for (const auto& outer : passes)
	{
		for (const auto& inner : passes)
		{
			if (outer == inner)
				continue;

			std::vector<size_t> intersection;
			// #TODO: We can stop as soon as there's a single intersection, std::set_intersection will find all intersections.
			std::set_intersection(outer->view.writes.begin(), outer->view.writes.end(), inner->view.reads.begin(), inner->view.reads.end(), intersection.begin());
			
			// If there's a write-to-read dependency, create an edge.
			if (intersection.size() > 0)
			{
				adjacencyList[outer].emplace_back(inner);
			}
		}
	}
}

void RenderGraph::DepthFirstSearch(size_t node, std::vector<bool>& visited, std::stack<bool>& stack)
{
	if (visited[node])
		return;

	visited[node] = true;

	for (const auto adjacentNode : adjacencyList[node])
	{
		DepthFirstSearch(adjacentNode, visited, stack);
	}

	stack.push(node);
}

void RenderGraph::TopologicalSort()
{
	sorted.reserve(passes.size());  // Most passes are unlikely to be trimmed.

	std::vector<bool> visited(passes.size(), 0);
	std::stack<bool> stack;

	for (const auto& pass : passes)
	{
		if (!visited[pass])
		{
			DepthFirstSearch(pass, visited, stack);
		}
	}

	while (stack.size() > 0)
	{
		sorted.push_back(stack.top());
		stack.pop();
	}
}

void RenderGraph::BuildDepthMap()
{
	for (const auto pass : sorted)
	{
		for (const auto adjacentPass : adjacencyList[pass])
		{
			if (depthMap[adjacentPass] < depthMap[pass] + 1)
			{
				depthMap[adjacentPass] = depthMap[pass] + 1;
			}
		}
	}
}