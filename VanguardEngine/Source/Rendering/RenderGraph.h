// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderPass.h>

class RenderGraph
{
private:
	size_t TagCounter = 0;

	// Passes should be stored in a sparse container so we don't shuffle, since we're returning raw addresses in AddPass().

public:
	size_t GetNextResourceTag() noexcept
	{
		return TagCounter++;
	}

	RenderPass& AddPass(const std::wstring_view Name);

	bool Build();
	void Execute();
};