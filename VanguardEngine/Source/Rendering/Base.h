// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Utility/ResourcePtr.h>

#include <DirectXMath.h>

using namespace DirectX;

using DescriptorHandle = uint64_t;

struct Vertex
{
	XMFLOAT3 Position;
	
	// #TODO: Normal, UVs, etc?
};