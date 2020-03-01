// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderGraphPass.h>

class RenderGraph
{
public:
	void AddPass(RenderGraphPass&& Pass);

	bool Build();
	bool Compile();
	void Execute();
};