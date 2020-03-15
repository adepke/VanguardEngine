// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraph.h>
#include <Rendering/CommandList.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>

bool RenderGraph::Validate()
{
	if (std::find_if(ResourceWrites.begin(), ResourceWrites.end(), [](const auto& Pair) { return Pair.second == RGUsage::SwapChain }) == ResourceWrites.end())
		return false;  // We need to have at least one pass that writes to the swap chain.

	auto CheckReadOnly = [](RGUsage Usage) { return
		Usage == RGUsage::ConstantBuffer ||
		Usage == RGUsage::VertexBuffer ||
		Usage == RGUsage::IndexBuffer;
	};
	auto CheckWriteOnly = [](RGUsage Usage) { return
		Usage == RGUsage::SwapChain;
	};

	for (const auto [ResourceTag, Usage] : ResourceReads)
	{
		if (CheckWriteOnly(Usage))
			return false;
	}

	for (const auto [ResourceTag, Usage] : ResourceWrites)
	{
		if (CheckReadOnly(Usage))
			return false;
	}

	return true;
}

void RenderGraph::Traverse(size_t ResourceTag)
{
	for (PassIndex : PassesThatWriteToResourceTag)
	{
		for (Tag : ResourceThatPassIndexWritesTo)
		{
			Traverse(Tag);
		}

		PassPipeline.push_back(PassIndex);
	}
}

std::stack<std::unique_ptr<RenderPass>> RenderGraph::Serialize()
{
	PassPipeline.reserve(Passes.size());  // Unlikely that a significant number of passes are going to be trimmed.

	const size_t SwapChainTag = std::find_if(ResourceWrites.begin(), ResourceWrites.end(), [](const auto& Pair) { return Pair.second == RGUsage::SwapChain })->first;
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
	PassCommands.reserve(PassPipeline.size());

	// #TODO: Parallel recording.
	size_t Index = 0;
	for (auto PassIndex : PassPipeline)
	{
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

	for (auto& List : PassCommands)
	{
		List->FlushBarriers();
		List->Close();
	}

	Device->GetDirectQueue()->ExecuteCommandLists(static_cast<UINT>(PassCommands.size()), PassCommands.data());
}