// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Utility/ResourcePtr.h>

#include <DirectXMath.h>

using namespace DirectX;

struct Vertex
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT2 UV;
	XMFLOAT3 Tangent;
	XMFLOAT3 Bitangent;
};

// #TODO: Temporary globals.
inline XMMATRIX GlobalViewMatrix;
inline XMMATRIX GlobalProjectionMatrix;