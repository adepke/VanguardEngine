// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <memory>

#include <Core/Windows/DirectX12Minimal.h>

struct GPUBuffer;

struct TransitionBarrier
{
	std::weak_ptr<GPUBuffer> Resource;
	D3D12_RESOURCE_STATES State;
};