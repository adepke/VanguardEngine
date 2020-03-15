// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderPass.h>
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
	size_t TagCounter = 0;

	std::vector<std::unique_ptr<RenderPass>> Passes;
	std::vector<size_t> PassPipeline;
	std::unordered_map<size_t, RGUsage> ResourceReads;  // Maps resource tag to usage.
	std::unordered_map<size_t, RGUsage> ResourceWrites;  // Maps resource tag to usage.

	bool Validate();  // Ensures we have a valid graph.
	void Traverse(size_t ResourceTag);  // Walk up the dependency chain, recursively adding passes to the stack. 
	std::stack<std::unique_ptr<RenderPass>> Serialize();  // Serializes to an optimized linear pipeline.

public:  // Utilities for render passes.
	void AddResourceRead(size_t PassIndex, size_t ResourceTag, RGUsage Usage) { ResourceReads[ResourceTag] = Usage; }
	void AddResourceWrite(size_t PassIndex, size_t ResourceTag, RGUsage Usage) { ResourceReads[ResourceTag] = Usage; }

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