// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraphResolver.h>

#include <vector>
#include <memory>
#include <stack>

class RenderDevice;
struct Buffer;
struct Texture;

class RenderGraph
{
private:
	RenderDevice* Device;
	size_t TagCounter = 0;

	std::vector<std::unique_ptr<RenderPass>> Passes;
	std::unordered_map<RGUsage, size_t> ResourceReads;  // Maps usage to pass index.
	std::unordered_map<RGUsage, size_t> ResourceWrites;  // Maps usage to pass index.

	bool Validate();  // Ensures we have a valid graph.
	std::stack<std::unique_ptr<RenderPass>> Serialize();  // Serializes to an optimized linear pipeline.

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