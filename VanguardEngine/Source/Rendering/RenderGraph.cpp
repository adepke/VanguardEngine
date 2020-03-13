// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraph.h>
#include <Rendering/CommandList.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>

bool RenderGraph::Validate()
{
	if (ResourceWrites.find(RGUsage::SwapChain) == ResourceWrites.end())
		return false;  // We need to have at least one pass that writes to the swap chain.

	auto CheckReadOnly = [](RGUsage Usage) { return
		Usage == RGUsage::ConstantBuffer ||
		Usage == RGUsage::VertexBuffer ||
		Usage == RGUsage::IndexBuffer;
	};
	auto CheckWriteOnly = [](RGUsage Usage) { return
		Usage == RGUsage::SwapChain;
	};

	for (const auto [Usage, PassIndex] : ResourceReads)
	{
		if (CheckWriteOnly(Usage))
			return false;
	}

	for (const auto [Usage, PassIndex] : ResourceWrites)
	{
		if (CheckReadOnly(Usage))
			return false;
	}

	return true;
}

std::stack<std::unique_ptr<RenderPass>> RenderGraph::Serialize()
{

}

RenderPass& RenderGraph::AddPass(const std::wstring& Name)
{
	for (const auto& Pass : Passes)
	{
		VGAssert(Pass->Name != Name, "Attempted to add multiple passes with the same name!");
	}

	auto a = std::make_unique<RenderPass>(*this, Name);
	return **Passes.begin();

	//return *Passes.emplace_back(std::make_unique<RenderPass>(*this, Name));
}

void RenderGraph::Build()
{
	VGAssert(Validate(), "Failed to validate render graph!");

	// Serialize the execution pipeline, we need to do this to resolve resource state and barriers.
	auto PassPipeline = std::move(Serialize());
}

void RenderGraph::Execute()
{
	// Record pass commands.

	CommandList* PreviousList;  // The most recent list that used this resource.
	std::shared_ptr<Buffer> TargetResource;  // The resource we're looking at. Comes from pass inputs.
	D3D12_RESOURCE_STATES NewResourceState;  // A new resource state derived from the usage.

	// The state changed, which means we need to inject a barrier at the end of the last list to use the resource. #TODO: Place a split barrier start
	// at the end of that list, and a split barrier beginning at the current list which needs it in a different state.
	if (TargetResource->State != NewResourceState)
	{
		PreviousList->TransitionBarrier(TargetResource, NewResourceState);
	}

	// For each list, list->FlushBarriers();

	// For each list, list->Close();

	//Device->GetDirectQueue()->ExecuteCommandLists(NumLists, DirectLists);
}