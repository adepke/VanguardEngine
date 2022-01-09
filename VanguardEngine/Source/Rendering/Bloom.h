// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/PipelineState.h>
#include <Rendering/RenderGraphResource.h>

class RenderDevice;
class RenderGraph;

class Bloom
{
private:
	RenderDevice* device;

	PipelineState extractState;
	PipelineState downsampleState;
	PipelineState compositionState;

	uint32_t bloomPasses = 0;

	void AddDownsamplePass(RenderGraph& graph, RenderResource input);
	void AddUpsamplePass();
	void AddContributionPass();

public:
	void Initialize(RenderDevice* inDevice);
	void Render(RenderGraph& graph, RenderResource hdrSource);
};