// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraph.h>

void RenderGraph::Build()
{
	// Trim passes that don't have their outputs fed into any other passes.
	// Next, order the passes in order to optimize execution (avoid running
	// passes that depend on each other right after one another). We have
	// to be extremely careful of maintaining dependency ordering, otherwise
	// we might end up using a resource before it is lazy-created.
}

void RenderGraph::Execute()
{
	// First start by recording command lists in parallel.
	// After they have all finished, we can batch execute them
	// or execute in small groups as we go to keep the GPU busy
	// more often. For now just batch execute all of them and
	// profile, the GPU might be fully loaded with work from
	// previous frames/async compute.

	// Serialize the execution pipeline, we need to do this to resolve resource state and barriers.

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