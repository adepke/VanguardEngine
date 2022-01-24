// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Utility/ResourcePtr.h>

#include <DirectXMath.h>
#define NOMINMAX  // #TEMP: Fix Windows.h leaking.
#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// #TODO: Temporary globals.
inline XMMATRIX globalViewMatrix;
inline XMMATRIX globalProjectionMatrix;