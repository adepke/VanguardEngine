// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Utility/ResourcePtr.h>

#include <DirectXMath.h>

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
	XMFLOAT3 tangent;
	XMFLOAT3 bitangent;
};

// #TODO: Temporary globals.
inline XMMATRIX globalViewMatrix;
inline XMMATRIX globalProjectionMatrix;