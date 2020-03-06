// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderGraph.h>

bool RenderGraph::Build()
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

	//Device->GetDirectQueue()->ExecuteCommandLists(NumLists, DirectLists);
}