// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <memory>

#include <Core/Windows/DirectX12Minimal.h>

struct Buffer;

struct TransitionBarrier
{
	std::weak_ptr<Buffer> Resource;
	D3D12_RESOURCE_STATES State;
};