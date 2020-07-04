// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/RenderGraphResource.h>

#include <memory>
#include <string_view>
#include <vector>
#include <functional>

class RenderGraph;
struct RGBufferDescription;
struct RGTextureDescription;
class CommandList;
class RGResolver;

// #TODO: Passes can either read or write to a single resource. Add the option to read and write to a resource?

class RenderPass
{
	friend class RenderGraph;

private:
	RenderGraph& graph;
	const char* staticName;  // Stable name that is safe to send to external profiling services.
	size_t index;
	std::vector<size_t> reads;
	std::vector<size_t> writes;

	std::function<void(RGResolver&, CommandList&)> execution;

public:
	RenderPass(RenderGraph& inGraph, const std::string_view inStaticName, size_t inIndex) : graph(inGraph), staticName(inStaticName.data()), index(inIndex) {}
	RenderPass(const RenderPass&) = delete;
	RenderPass(RenderPass&&) noexcept = default;

	RenderPass& operator=(const RenderPass&) = delete;
	RenderPass& operator=(RenderPass&&) noexcept = default;

	size_t CreateResource(const RGBufferDescription& description, const std::wstring& name);
	size_t CreateResource(const RGTextureDescription& description, const std::wstring& name);
	// When a pass declares a read/write on a resource, it will assume it performs that action on the entire resource,
	// for the entire execution of the pass.
	void ReadResource(size_t resourceTag, RGUsage usage);  // #TODO: Most resources aren't mutable state-wise, so we should create different paths.
	void WriteResource(size_t resourceTag, RGUsage usage);

	void Bind(std::function<void(RGResolver&, CommandList&)> function) { execution = std::move(function); }
};