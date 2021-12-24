// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/RenderGraphResource.h>
#include <Rendering/RenderGraphResourceManager.h>

#include <set>
#include <functional>
#include <optional>
#include <string_view>

class CommandList;

enum class ResourceBind
{
	CBV,
	SRV,
	UAV,
	DSV,
	Indirect,
	Common
};

enum class OutputBind
{
	RTV,
	DSV
};

enum class LoadType
{
	Preserve,
	Clear
};

enum class ExecutionQueue
{
	Graphics,
	Compute
};

class RenderPass
{
public:
	std::string_view stableName;
	ExecutionQueue queue;

	std::set<RenderResource> reads;
	std::set<RenderResource> writes;

	std::unordered_map<RenderResource, ResourceBind> bindInfo;
	std::unordered_map<RenderResource, std::pair<OutputBind, LoadType>> outputBindInfo;

private:
	RenderGraphResourceManager* resourceManager;
	std::function<void(CommandList&, RenderGraphResourceManager&)> binding;

#if !BUILD_RELEASE
	// Used for validation.
	std::set<RenderResource> creates;
	std::set<RenderResource> outputs;
#endif

public:
	RenderPass(RenderGraphResourceManager* inResourceManager, std::string_view name, ExecutionQueue execution);

	const RenderResource Create(TransientBufferDescription description, const std::wstring& name);
	const RenderResource Create(TransientTextureDescription description, const std::wstring& name);
	void Read(const RenderResource resource, ResourceBind bind);
	void Write(const RenderResource resource, ResourceBind bind);
	void Output(const RenderResource resource, OutputBind bind, LoadType load);
	void Bind(std::function<void(CommandList&, RenderGraphResourceManager&)>&& function) noexcept;

	void Validate() const;  // Internal validation invoked from the graph. Checks for conditions after completing the pass setup.
	void Execute(CommandList& list, RenderGraphResourceManager& resources) const;
};

inline RenderPass::RenderPass(RenderGraphResourceManager* inResourceManager, std::string_view name, ExecutionQueue execution)
	: resourceManager(inResourceManager), stableName(name), queue(execution)
	{}

inline const RenderResource RenderPass::Create(TransientBufferDescription description, const std::wstring& name)
{
	const auto resource = resourceManager->AddResource(description, name);
	writes.emplace(resource);

#if !BUILD_RELEASE
	creates.emplace(resource);
#endif

	return resource;
}

inline const RenderResource RenderPass::Create(TransientTextureDescription description, const std::wstring& name)
{
	const auto resource = resourceManager->AddResource(description, name);
	writes.emplace(resource);

#if !BUILD_RELEASE
	creates.emplace(resource);
#endif

	return resource;
}

inline void RenderPass::Read(const RenderResource resource, ResourceBind bind)
{
	reads.emplace(resource);
	bindInfo[resource] = bind;
}

inline void RenderPass::Write(const RenderResource resource, ResourceBind bind)
{
	writes.emplace(resource);
	bindInfo[resource] = bind;
}

inline void RenderPass::Output(const RenderResource resource, OutputBind bind, LoadType load)
{
	writes.emplace(resource);
	outputBindInfo[resource] = std::make_pair(bind, load);

#if !BUILD_RELEASE
	outputs.emplace(resource);
#endif
}

inline void RenderPass::Bind(std::function<void(CommandList&, RenderGraphResourceManager&)>&& function) noexcept
{
	binding = std::move(function);
}

inline void RenderPass::Validate() const
{
#if !BUILD_RELEASE
	// Check that no resources are read and written in this pass. A write implies a read.
	std::vector<RenderResource> overlap;
	std::set_intersection(reads.cbegin(), reads.cend(), writes.cbegin(), writes.cend(), std::back_inserter(overlap));
	VGAssert(overlap.size() == 0, "Pass validation failed in '%s': Cannot read and write to a single resource.", stableName.data());
	overlap.clear();

	// Check that no created resources are read in this pass.
	std::set_intersection(reads.cbegin(), reads.cend(), creates.cbegin(), creates.cend(), std::back_inserter(overlap));
	VGAssert(overlap.size() == 0, "Pass validation failed in '%s': Cannot read resources created in the same pass.", stableName.data());

	// Check that created resources that are written are not outputs, and that created resources without being written are outputs.

	for (const auto resource : creates)
	{
		if (bindInfo.contains(resource))
		{
			VGAssert(!outputs.contains(resource), "Pass validation failed in '%s': Resources created and written in this pass cannot be outputs.", stableName.data());
		}

		else
		{
			VGAssert(outputs.contains(resource), "Pass validation failed in '%s': Resources created and not written in this pass must be outputs.", stableName.data());
		}
	}

	// Check that the number of render targets doesn't exceed D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT.
	// Check that we have at most one depth stencil output.

	size_t renderTargetCount = 0;
	size_t depthStencilCount = 0;

	for (const auto& [resource, info] : outputBindInfo)
	{
		if (info.first == OutputBind::RTV)
		{
			++renderTargetCount;
		}

		else if (info.first == OutputBind::DSV)
		{
			++depthStencilCount;
		}
	}

	VGAssert(renderTargetCount <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, "Pass validation failed in '%s': Attempted to output to more render targets than supported.", stableName.data());
	VGAssert(depthStencilCount <= 1, "Pass validation failed in '%s': Cannot have more than one depth stencil output.", stableName.data());
#endif
}

inline void RenderPass::Execute(CommandList& list, RenderGraphResourceManager& resources) const
{
	binding(list, resources);
}