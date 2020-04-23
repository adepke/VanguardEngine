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
	RenderGraph& Graph;
	const char* StaticName;  // Stable name that is safe to send to external profiling services.
	size_t Index;
	std::vector<size_t> Reads;
	std::vector<size_t> Writes;

	std::function<void(RGResolver&, CommandList&)> Execution;

public:
	RenderPass(RenderGraph& InGraph, const std::string_view InStaticName, size_t InIndex) : Graph(InGraph), StaticName(InStaticName.data()), Index(InIndex) {}
	RenderPass(const RenderPass&) = delete;
	RenderPass(RenderPass&&) noexcept = default;

	RenderPass& operator=(const RenderPass&) = delete;
	RenderPass& operator=(RenderPass&&) noexcept = default;

	size_t CreateResource(const RGBufferDescription& Description, const std::wstring& Name);
	size_t CreateResource(const RGTextureDescription& Description, const std::wstring& Name);
	// When a pass declares a read/write on a resource, it will assume it performs that action on the entire resource,
	// for the entire execution of the pass.
	void ReadResource(size_t ResourceTag, RGUsage Usage);  // #TODO: Most resources aren't mutable state-wise, so we should create different paths.
	void WriteResource(size_t ResourceTag, RGUsage Usage);

	void Bind(std::function<void(RGResolver&, CommandList&)> Function) { Execution = std::move(Function); }
};