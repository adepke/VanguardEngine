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
struct CommandList;
class RGResolver;

// #TODO: Passes can either read or write to a single resource. Add the option to read and write to a resource?

class RenderPass
{
	friend class RenderGraph;

private:
	RenderGraph& Graph;
	std::wstring Name;
	size_t Index;
	std::vector<size_t> Reads;
	std::vector<size_t> Writes;

	std::function<void(RGResolver&, CommandList&)> Execution;

public:
	RenderPass(RenderGraph& InGraph, const std::wstring& InName, size_t InIndex) : Graph(InGraph), Name(InName), Index(InIndex) {}
	RenderPass(const RenderPass&) = delete;
	RenderPass(RenderPass&&) noexcept = default;

	RenderPass& operator=(const RenderPass&) = delete;
	RenderPass& operator=(RenderPass&&) noexcept = default;

	size_t CreateResource(const RGBufferDescription& Description, const std::wstring_view Name);
	size_t CreateResource(const RGTextureDescription& Description, const std::wstring_view Name);
	// When a pass declares a read/write on a resource, it will assume it performs that action on the entire resource,
	// for the entire execution of the pass.
	void ReadResource(size_t ResourceTag, RGUsage Usage);  // #TODO: Most resources aren't mutable state-wise, so we should create different paths.
	void WriteResource(size_t ResourceTag, RGUsage Usage);

	void Bind(std::function<void(RGResolver&, CommandList&)> Function) { Execution = std::move(Function); }
};