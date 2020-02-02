// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#if (defined(__d3d12_h__) || defined(__d3dcommon_h__)) && !defined(DIRECTX12MINIMAL)
// #TEMP: We can't use this until D3D12MemoryAllocator doesn't include d3d12 files manually.
//#error "Included DirectX12 files manually, include this file instead."
#endif

#define DIRECTX12MINIMAL

#define NOMINMAX
//#include <ole2.h>
#include <Windows.h>

#define _INC_WINDOWS  // Prevents including windows.h from rpc.h
#define COM_NO_WINDOWS_H  // Prevents including windows.h from d3d12.h and d3dcommon.h

#include <d3d12.h>
#include <d3dcommon.h>
#include <dxgi.h>