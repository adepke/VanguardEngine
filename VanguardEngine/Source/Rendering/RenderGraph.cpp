// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraph.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>

std::optional<size_t> RenderGraph::FindUsage(RGUsage Usage)
{
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
	if (!FindUsage(RGUsage::SwapChain))
	{
		return false;  // We need to have at least one pass that writes to the swap chain.
	}

	auto CheckReadOnly = [](RGUsage Usage) { return
		Usage == RGUsage::ConstantBuffer ||
		Usage == RGUsage::VertexBuffer ||
		Usage == RGUsage::IndexBuffer;
	};
	auto CheckWriteOnly = [](RGUsage Usage) { return
		Usage == RGUsage::SwapChain;
	};

	for (const auto& [ResourceTag, UsageData] : ResourceUsages)
	{
		for (const auto [PassIndex, Usage] : UsageData.PassUsage)
		{
			// Ensure we don't read from this resource while in a write-only usage.
			if (CheckWriteOnly(Usage) && ResourceDependencies[ResourceTag].ReadingPasses.count(PassIndex))
				return false;

			// Ensure we don't write to this resource while in a read-only usage.
			if (CheckReadOnly(Usage) && ResourceDependencies[ResourceTag].WritingPasses.count(PassIndex))
				return false;
		}
	}

	return true;
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
			Traverse(Tag);
		}

		PassPipeline.push_back(PassIndex);
	}
}

void RenderGraph::Serialize()
{
	PassPipeline.reserve(Passes.size());  // Unlikely that a significant number of passes are going to be trimmed.

	const size_t SwapChainTag = *FindUsage(RGUsage::SwapChain);
	Traverse(SwapChainTag);

	// #TODO: Optimize the pipeline.
}

RenderPass& RenderGraph::AddPass(const std::wstring& Name)
{
	for (const auto& Pass : Passes)
	{
		VGAssert(Pass->Name != Name, "Attempted to add multiple passes with the same name!");
	}

	return *Passes.emplace_back(std::make_unique<RenderPass>(*this, Name, Passes.size()));
}

void RenderGraph::Build()
{
	VGAssert(Validate(), "Failed to validate render graph!");

	// Serialize the execution pipeline, we need to do this to resolve resource state and barriers.
	Serialize();
}

void RenderGraph::Execute()
{
	VGAssert(PassPipeline.size(), "Render graph is empty, ensure you built the graph before execution.");

	RGResolver Resolver;

	std::vector<CommandList> PassCommands;
	PassCommands.resize(PassPipeline.size());

	// #TODO: Parallel recording.
	size_t Index = 0;
	for (auto PassIndex : PassPipeline)
	{
		PassCommands[Index].Create(*Device, D3D12_COMMAND_LIST_TYPE_DIRECT);

		Passes[PassIndex]->Execution(Resolver, PassCommands[Index]);
		++Index;
	}

	CommandList* PreviousList;  // The most recent list that used this resource.
	std::shared_ptr<Buffer> TargetResource;  // The resource we're looking at. Comes from pass inputs.
	D3D12_RESOURCE_STATES NewResourceState;  // A new resource state derived from the usage.

	// The state changed, which means we need to inject a barrier at the end of the last list to use the resource. #TODO: Place a split barrier start
	// at the end of that list, and a split barrier beginning at the current list which needs it in a different state.
	if (TargetResource->State != NewResourceState)
	{
		PreviousList->TransitionBarrier(TargetResource, NewResourceState);
	}

	std::vector<ID3D12CommandList*> PassLists;
	PassLists.reserve(PassCommands.size());

	for (auto& List : PassCommands)
	{
		List.FlushBarriers();
		List.Close();
		PassLists.emplace_back(List.Native());
	}

	Device->GetDirectQueue()->ExecuteCommandLists(static_cast<UINT>(PassLists.size()), PassLists.data());
}