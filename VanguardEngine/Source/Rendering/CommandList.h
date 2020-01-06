// Copyright (c) 2019-2020 Andrew Depke

#pragma once

struct CommandList
{
	void AddResourceBarrier(const std::shared_ptr<GPUBuffer>& Resource, TransitionBarrier Barrier);
};