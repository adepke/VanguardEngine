// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <string_view>

enum class ExecutionQueue
{
	Graphics,
	Compute
};

class RenderGraph
{
public:
	size_t Import(const std::shared_ptr<Buffer>& resource);
	size_t Import(const std::shared_ptr<Texture>& resource);

	RenderPass& AddPass(const std::string_view stableName, ExecutionQueue executionQueue);

	void Build();
	void Execute();
};