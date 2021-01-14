// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Utility/ResourcePtr.h>

#include <DirectXMath.h>
#define NOMINMAX  // #TEMP: Fix Windows.h leaking.
#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

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