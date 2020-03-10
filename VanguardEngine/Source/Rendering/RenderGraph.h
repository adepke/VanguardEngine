// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/RenderPass.h>

class RenderGraph
{
private:
	RenderDevice* Device;
	size_t TagCounter = 0;

	// Passes should be stored in a sparse container so we don't shuffle, since we're returning raw addresses in AddPass().

public:
	RenderGraph(RenderDevice* InDevice) : Device(InDevice) {}

	size_t GetNextResourceTag() noexcept
	{
		return TagCounter++;
	}

	// Imports an externally-managed resource, such as the swap chain surface.
	size_t ImportResource(std::shared_ptr<Buffer>& Resource);
	size_t ImportResource(std::shared_ptr<Texture>& Resource);

	RenderPass& AddPass(const std::wstring_view Name);

	void Build();
	void Execute();
};