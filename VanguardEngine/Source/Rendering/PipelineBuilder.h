// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/PipelineState.h>

#include <unordered_map>

class PipelineBuilder
{
private:
	std::unordered_map<std::string, PipelineState> pipelines;

public:
	void AddGraphicsState(RenderDevice* device, const std::string& name, const PipelineStateDescription& description)
	{
		VGAssert(!pipelines.contains(name), "Duplicate pipeline name.");

		pipelines[name].Build(*device, description);
	}

	//void AddComputeState();  // #TODO: Implement compute pipelines.

	const PipelineState& operator[](const std::string& name);
};

inline const PipelineState& PipelineBuilder::operator[](const std::string& name)
{
	VGAssert(pipelines.contains(name), "Attempted to use unregistered pipeline: '%s'", name);

	return pipelines[name];
}