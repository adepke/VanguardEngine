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
	PipelineState upsampleState;

	uint32_t bloomPasses = 0;

	void AddDownsamplePass(RenderGraph& graph, RenderResource input);
	void AddUpsamplePass();
	void AddContributionPass();

public:
	// Values from: https://www.froyok.fr/blog/2021-12-ue4-custom-bloom/
	float internalBlend = 0.85f;
	float intensity = 0.3f;

public:
	void Initialize(RenderDevice* inDevice);
	void Render(RenderGraph& graph, RenderResource hdrSource);
};