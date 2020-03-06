// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <memory>
#include <string_view>
#include <vector>
#include <functional>

class RenderGraph;
struct RGBufferDescription;
struct RGTextureDescription;
struct CommandList;

class RenderPass
{
	friend class RenderGraph;

private:
	std::weak_ptr<RenderGraph> Graph;
	std::vector<size_t> Reads;
	std::vector<size_t> Writes;

protected:
	RenderPass() = default;  // Only allow pass creation from RenderGraph.

public:
	RenderPass(const RenderPass&) = delete;
	RenderPass(RenderPass&&) noexcept = default;

	RenderPass& operator=(const RenderPass&) = delete;
	RenderPass& operator=(RenderPass&&) noexcept = default;

	size_t CreateResource(const RGBufferDescription& Description, const std::wstring_view Name);
	size_t CreateResource(const RGTextureDescription& Description, const std::wstring_view Name);
	void ReadResource(size_t ResourceTag);
	void WriteResource(size_t ResourceTag);

	void BindExecution(std::function<void(CommandList&)> Function);
};