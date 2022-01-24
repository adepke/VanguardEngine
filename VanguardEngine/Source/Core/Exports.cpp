// Copyright (c) 2019-2022 Andrew Depke

#include <cstdint>

extern "C"
{
	// DirectX Agility SDK
	__declspec(dllexport) extern const uint32_t D3D12SDKVersion = 4;
	__declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
};