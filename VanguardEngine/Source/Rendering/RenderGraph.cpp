// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraph.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>

#include <utility>
#include <limits>
#include <cstring>

D3D12_RESOURCE_STATES UsageToState(const std::shared_ptr<Buffer>& resource, RGUsage usage, bool write)
{
	D3D12_RESOURCE_STATES readState{};
	if (!write)
	{
		if (resource->description.bindFlags & BindFlag::ConstantBuffer) readState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		else if (resource->description.bindFlags & BindFlag::VertexBuffer) readState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (resource->description.bindFlags & BindFlag::IndexBuffer) readState |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (resource->description.bindFlags & BindFlag::ShaderResource) readState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}

	switch (usage)
	{
	case RGUsage::Default: return write ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : readState;
	}

	VGEnsure(false, "Buffer resource cannot have specified usage.");
	return {};
}

D3D12_RESOURCE_STATES UsageToState(const std::shared_ptr<Texture>& resource, RGUsage usage, bool write)
{
	D3D12_RESOURCE_STATES readAdditiveState{};
	if (!write)
	{
		if (resource->description.bindFlags & BindFlag::ShaderResource) readAdditiveState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}

	switch (usage)
	{
	case RGUsage::Default: return write ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : readAdditiveState;
	case RGUsage::RenderTarget: return write ? D3D12_RESOURCE_STATE_RENDER_TARGET : readAdditiveState;
	case RGUsage::DepthStencil: return write ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_DEPTH_READ | readAdditiveState;
	case RGUsage::BackBuffer: return write ? D3D12_RESOURCE_STATE_RENDER_TARGET : readAdditiveState;
	}

	VGEnsure(false, "Texture resource cannot have specified usage.");
	return {};
}

std::optional<size_t> RenderGraph::FindUsage(RGUsage usage)
{
	VGScopedCPUStat("Render Graph Find Usage");

	 const auto iter = std::find_if(resourceUsages.begin(), resourceUsages.end(), [usage](const auto& Pair)
		{
			const auto& usageMap = Pair.second.passUsage;  // Mapping of pass indexes to usages.

			return std::find_if(usageMap.begin(), usageMap.end(), [usage](const auto& Pair)
				{
					return Pair.second == usage;
				}) != usageMap.end();
		});

	 if (iter != resourceUsages.end())
	 {
		 return iter->first;
	 }

	 return std::nullopt;
}

bool RenderGraph::Validate()
{
	VGScopedCPUStat("Render Graph Validate");

	if (!FindUsage(RGUsage::BackBuffer))
	{
		return false;  // We need to have at least one pass that writes to the back buffer.
	}

	return true;
}

std::unordered_set<size_t> RenderGraph::FindReadResources(size_t passIndex)
{
	std::unordered_set<size_t> result;

	for (const auto& [resourceTag, dependency] : resourceDependencies)
	{
		if (dependency.readingPasses.count(passIndex))
			result.insert(resourceTag);
	}

	return std::move(result);
}

std::unordered_set<size_t> RenderGraph::FindWrittenResources(size_t passIndex)
{
	std::unordered_set<size_t> result;

	for (const auto& [resourceTag, dependency] : resourceDependencies)
	{
		if (dependency.writingPasses.count(passIndex))
			result.insert(resourceTag);
	}

	return std::move(result);
}

void RenderGraph::Traverse(std::unordered_set<size_t>& trackedPasses, size_t resourceTag)
{
	for (const size_t passIndex : resourceDependencies[resourceTag].writingPasses)
	{
		// If this pass has been added to the pipeline by a previous dependency, skip it.
		if (trackedPasses.count(passIndex))
			continue;

		trackedPasses.insert(passIndex);

		for (const size_t tag : FindWrittenResources(passIndex))
		{
			// We don't want an infinite loop.
			if (tag != resourceTag)
				Traverse(trackedPasses, tag);
		}

		passPipeline.push_back(passIndex);
	}
}

void RenderGraph::Serialize()
{
	VGScopedCPUStat("Render Graph Serialize");

	passPipeline.reserve(passes.size());  // Unlikely that a significant number of passes are going to be trimmed.

	const size_t swapChainTag = *FindUsage(RGUsage::BackBuffer);  // #TODO: This will only work if we have 1 pass write to the swap chain.
	std::unordered_set<size_t> trackedPasses;
	Traverse(trackedPasses, swapChainTag);

	// #TODO: Optimize the pipeline.
}

void RenderGraph::InjectStateBarriers(std::vector<std::shared_ptr<CommandList>>& lists)
{
	VGScopedCPUStat("Render Graph Barriers");

	std::unordered_map<size_t, std::pair<D3D12_RESOURCE_STATES, size_t>> stateMap;  // Maps resource tag to the state and which pass last modified it.
	constexpr auto invalidPassIndex = std::numeric_limits<size_t>::max();

	for (const auto& [resourceTag, usage] : resourceUsages)
	{
		stateMap.emplace(resourceTag, std::make_pair(resolver.DetermineInitialState(resourceTag), invalidPassIndex));
	}

	const auto checkState = [this, &stateMap, &lists](size_t passIndex, const auto& resources, bool write)
	{
		for (size_t resource : resources)
		{
			auto resourceBuffer = resolver.FetchAsBuffer(resource);
			auto resourceTexture = resolver.FetchAsTexture(resource);

			VGAssert(resourceBuffer || resourceTexture, "Failed to fetch the resource.");

			// Dynamic resources don't ever transition states, so we can skip them.
			if (resourceBuffer && resourceBuffer->description.updateRate == ResourceFrequency::Dynamic) continue;
			if (resourceTexture && resourceTexture->description.updateRate == ResourceFrequency::Dynamic) continue;

			// #TEMP: We can't use PassIndex directly, since it might be a size_t::max.
			D3D12_RESOURCE_STATES newState;

			if (resourceBuffer)
				newState = UsageToState(resourceBuffer, resourceUsages[resource].passUsage[passIndex], write);
			else
				newState = UsageToState(resourceTexture, resourceUsages[resource].passUsage[passIndex], write);

			if (newState != stateMap[resource].first)
			{
				// The state changed, which means we need to inject a barrier at the end of the last list to use the resource.
				// #TODO: Potentially use split barriers.

				if (resourceBuffer)
					lists[passIndex]->TransitionBarrier(resourceBuffer, newState);  // #TEMP: After reordering we can't access lists like this, need a map.
				else
					lists[passIndex]->TransitionBarrier(resourceTexture, newState);

				stateMap[resource] = { newState, passIndex };
			}
		}
	};

	for (size_t i = 0; i < passPipeline.size(); ++i)
	{
		auto reads = FindReadResources(passPipeline[i]);
		const auto writes = FindWrittenResources(passPipeline[i]);

		// We need to remove resource that are both read and write during this pass from the read-only list.
		for (auto iter = reads.begin(); iter != reads.end();)
		{
			if (writes.count(*iter))
				iter = reads.erase(iter);
			else
				++iter;
		}

		checkState(i, reads, false);
		checkState(i, writes, true);
	}

	// #TODO: Strongly consider looking ahead at sequential reading passes (up until a write pass) and merge their required
	// read state into the current barrier in order to avoid transitions from a read-only state to another read-only state.

	// Placed all the barriers, now flush them.
	for (auto& list : lists)
	{
		list->FlushBarriers();
	}
}

void RenderGraph::InjectRaceBarriers(std::vector<std::shared_ptr<CommandList>>& lists)
{
	// #TODO: Place UAV barriers when needed.

	// Placed all the barriers, now flush them.
	for (auto& list : lists)
	{
		list->FlushBarriers();
	}
}

RenderPass& RenderGraph::AddPass(const std::string_view staticName)
{
	VGScopedCPUStat("Render Graph Add Pass");

	for (const auto& pass : passes)
	{
		VGAssert(std::strcmp(pass->staticName, staticName.data()) != 0, "Attempted to add multiple passes with the same name!");
	}

	return *passes.emplace_back(std::make_unique<RenderPass>(*this, staticName, passes.size()));
}

void RenderGraph::Build()
{
	VGScopedCPUStat("Render Graph Build");

	VGAssert(Validate(), "Failed to validate render graph!");

	// Serialize the execution pipeline, we need to do this to resolve resource state and barriers.
	Serialize();

	resolver.BuildTransients(device, resourceDependencies, resourceUsages);
}

void RenderGraph::Execute()
{
	VGScopedCPUStat("Render Graph Execute");

	VGAssert(passPipeline.size(), "Render graph is empty, ensure you built the graph before execution.");

	std::vector<std::shared_ptr<CommandList>> passCommands;
	passCommands.resize(passPipeline.size());

	size_t index = 0;
	for (auto passIndex : passPipeline)
	{
		passCommands[index] = device->AllocateFrameCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);
		++index;
	}

	InjectStateBarriers(passCommands);

	// #TODO: Parallel recording.
	index = 0;
	for (auto passIndex : passPipeline)
	{
		VGScopedCPUStat(passes[passIndex]->staticName);

		passes[passIndex]->execution(resolver, *passCommands[index]);
		++index;
	}

	InjectRaceBarriers(passCommands);

	// Ensure the back buffer is in presentation state at the very end of the graph.
	passCommands[passCommands.size() - 1]->TransitionBarrier(resolver.FetchAsTexture(*FindUsage(RGUsage::BackBuffer)), D3D12_RESOURCE_STATE_PRESENT);

	std::vector<ID3D12CommandList*> passLists;
	passLists.reserve(passCommands.size() + 1);

	device->GetDirectList().FlushBarriers();
	device->GetDirectList().Close();
	passLists.emplace_back(device->GetDirectList().Native());  // Add the device's main draw list. #TEMP: Get rid of this monolithic list.

	for (auto& list : passCommands)
	{
		list->FlushBarriers();
		list->Close();
		passLists.emplace_back(list->Native());
	}

	device->GetDirectQueue()->ExecuteCommandLists(static_cast<UINT>(passLists.size()), passLists.data());
}