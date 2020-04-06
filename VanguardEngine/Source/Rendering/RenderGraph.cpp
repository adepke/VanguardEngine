// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraph.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>

#include <utility>
#include <limits>
#include <cstring>

D3D12_RESOURCE_STATES UsageToState(const std::shared_ptr<Buffer>& Resource, RGUsage Usage, bool Write)
{
	D3D12_RESOURCE_STATES ReadState{};
	if (!Write)
	{
		if (Resource->Description.BindFlags & BindFlag::ConstantBuffer) ReadState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		else if (Resource->Description.BindFlags & BindFlag::VertexBuffer) ReadState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (Resource->Description.BindFlags & BindFlag::IndexBuffer) ReadState |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (Resource->Description.BindFlags & BindFlag::ShaderResource) ReadState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}

	switch (Usage)
	{
	case RGUsage::Default: return Write ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : ReadState;
	}

	VGEnsure(false, "Buffer resource cannot have specified usage.");
	return {};
}

D3D12_RESOURCE_STATES UsageToState(const std::shared_ptr<Texture>& Resource, RGUsage Usage, bool Write)
{
	D3D12_RESOURCE_STATES ReadAdditiveState{};
	if (!Write)
	{
		if (Resource->Description.BindFlags & BindFlag::ShaderResource) ReadAdditiveState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}

	switch (Usage)
	{
	case RGUsage::Default: return Write ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : ReadAdditiveState;
	case RGUsage::RenderTarget: return Write ? D3D12_RESOURCE_STATE_RENDER_TARGET : ReadAdditiveState;
	case RGUsage::DepthStencil: return Write ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_DEPTH_READ | ReadAdditiveState;
	case RGUsage::BackBuffer: return Write ? D3D12_RESOURCE_STATE_RENDER_TARGET : ReadAdditiveState;
	}

	VGEnsure(false, "Texture resource cannot have specified usage.");
	return {};
}

std::optional<size_t> RenderGraph::FindUsage(RGUsage Usage)
{
	VGScopedCPUStat("Render Graph Find Usage");

	 const auto Iter = std::find_if(ResourceUsages.begin(), ResourceUsages.end(), [Usage](const auto& Pair)
		{
			const auto& UsageMap = Pair.second.PassUsage;  // Mapping of pass indexes to usages.

			return std::find_if(UsageMap.begin(), UsageMap.end(), [Usage](const auto& Pair)
				{
					return Pair.second == Usage;
				}) != UsageMap.end();
		});

	 if (Iter != ResourceUsages.end())
	 {
		 return Iter->first;
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

std::unordered_set<size_t> RenderGraph::FindReadResources(size_t PassIndex)
{
	std::unordered_set<size_t> Result;

	for (const auto& [ResourceTag, Dependency] : ResourceDependencies)
	{
		if (Dependency.ReadingPasses.count(PassIndex))
			Result.insert(ResourceTag);
	}

	return std::move(Result);
}

std::unordered_set<size_t> RenderGraph::FindWrittenResources(size_t PassIndex)
{
	std::unordered_set<size_t> Result;

	for (const auto& [ResourceTag, Dependency] : ResourceDependencies)
	{
		if (Dependency.WritingPasses.count(PassIndex))
			Result.insert(ResourceTag);
	}

	return std::move(Result);
}

void RenderGraph::Traverse(size_t ResourceTag)
{
	for (const size_t PassIndex : ResourceDependencies[ResourceTag].WritingPasses)
	{
		for (const size_t Tag : FindWrittenResources(PassIndex))
		{
			// We don't want an infinite loop.
			if (Tag != ResourceTag)
				Traverse(Tag);
		}

		PassPipeline.push_back(PassIndex);
	}
}

void RenderGraph::Serialize()
{
	VGScopedCPUStat("Render Graph Serialize");

	PassPipeline.reserve(Passes.size());  // Unlikely that a significant number of passes are going to be trimmed.

	const size_t SwapChainTag = *FindUsage(RGUsage::BackBuffer);  // #TODO: This will only work if we have 1 pass write to the swap chain.
	Traverse(SwapChainTag);

	// #TODO: Optimize the pipeline.
}

void RenderGraph::InjectBarriers(std::vector<CommandList>& Lists)
{
	VGScopedCPUStat("Render Graph Barriers");

	std::unordered_map<size_t, std::pair<D3D12_RESOURCE_STATES, size_t>> StateMap;  // Maps resource tag to the state and which pass last modified it.
	constexpr auto InvalidPassIndex = std::numeric_limits<size_t>::max();

	for (const auto& [ResourceTag, Usage] : ResourceUsages)
	{
		StateMap.emplace(ResourceTag, std::make_pair(Resolver.DetermineInitialState(ResourceTag), InvalidPassIndex));
	}

	const auto CheckState = [this, &StateMap, &Lists](size_t PassIndex, const auto& Resources, bool Write)
	{
		for (size_t Resource : Resources)
		{
			auto ResourceBuffer = Resolver.FetchAsBuffer(Resource);
			auto ResourceTexture = Resolver.FetchAsTexture(Resource);

			VGAssert(ResourceBuffer || ResourceTexture, "Failed to fetch the resource.");

			// #TEMP: We can't use PassIndex directly, since it might be a size_t::max.
			D3D12_RESOURCE_STATES NewState;

			if (ResourceBuffer)
				NewState = UsageToState(ResourceBuffer, ResourceUsages[Resource].PassUsage[PassIndex], Write);
			else
				NewState = UsageToState(ResourceTexture, ResourceUsages[Resource].PassUsage[PassIndex], Write);

			if (NewState != StateMap[Resource].first)
			{
				// The state changed, which means we need to inject a barrier at the end of the last list to use the resource.
				// #TODO: Potentially use split barriers.

				if (ResourceBuffer)
					Lists[PassIndex].TransitionBarrier(ResourceBuffer, NewState);  // #TEMP: After reordering we can't access lists like this, need a map.
				else
					Lists[PassIndex].TransitionBarrier(ResourceTexture, NewState);

				StateMap[Resource] = { NewState, PassIndex };
			}
		}
	};

	for (size_t Iter = 0; Iter < PassPipeline.size(); ++Iter)
	{
		const auto Reads = FindReadResources(PassPipeline[Iter]);
		const auto Writes = FindWrittenResources(PassPipeline[Iter]);

		CheckState(Iter, Reads, false);
		CheckState(Iter, Writes, true);
	}

	// Placed all the barriers, now flush them.
	for (auto& List : Lists)
	{
		List.FlushBarriers();
	}
}

RenderPass& RenderGraph::AddPass(const std::string_view StaticName)
{
	VGScopedCPUStat("Render Graph Add Pass");

	for (const auto& Pass : Passes)
	{
		VGAssert(std::strcmp(Pass->StaticName, StaticName.data()) != 0, "Attempted to add multiple passes with the same name!");
	}

	return *Passes.emplace_back(std::make_unique<RenderPass>(*this, StaticName, Passes.size()));
}

void RenderGraph::Build()
{
	VGScopedCPUStat("Render Graph Build");

	VGAssert(Validate(), "Failed to validate render graph!");

	// Serialize the execution pipeline, we need to do this to resolve resource state and barriers.
	Serialize();

	Resolver.BuildTransients(Device, ResourceDependencies, ResourceUsages);
}

void RenderGraph::Execute()
{
	VGScopedCPUStat("Render Graph Execute");

	VGAssert(PassPipeline.size(), "Render graph is empty, ensure you built the graph before execution.");

	std::vector<CommandList> PassCommands;
	PassCommands.resize(PassPipeline.size());

	// #TODO: Pool command lists in a ring buffer, extremely wasteful to recreate every frame.
	size_t Index = 0;
	for (auto PassIndex : PassPipeline)
	{
		PassCommands[Index].Create(*Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		++Index;
	}

	InjectBarriers(PassCommands);

	// #TODO: Parallel recording.
	Index = 0;
	for (auto PassIndex : PassPipeline)
	{
		VGScopedCPUStat(Passes[PassIndex]->StaticName);

		Passes[PassIndex]->Execution(Resolver, PassCommands[Index]);
		++Index;
	}

	// Ensure the back buffer is in presentation state at the very end of the graph.
	PassCommands[PassCommands.size() - 1].TransitionBarrier(Resolver.FetchAsTexture(*FindUsage(RGUsage::BackBuffer)), D3D12_RESOURCE_STATE_PRESENT);

	std::vector<ID3D12CommandList*> PassLists;
	PassLists.reserve(PassCommands.size() + 1);

	Device->GetDirectList().FlushBarriers();
	Device->GetDirectList().Close();
	PassLists.emplace_back(Device->GetDirectList().Native());  // Add the device's main draw list. #TEMP: Get rid of this monolithic list.

	for (auto& List : PassCommands)
	{
		List.Close();
		PassLists.emplace_back(List.Native());
	}

	Device->GetDirectQueue()->ExecuteCommandLists(static_cast<UINT>(PassLists.size()), PassLists.data());
}