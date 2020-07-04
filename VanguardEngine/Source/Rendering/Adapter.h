// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <Core/Windows/DirectX12Minimal.h>

class Adapter
{
private:
	ResourcePtr<IDXGIAdapter1> adapterResource;

public:
	auto* Native() const noexcept { return adapterResource.Get(); }

	void Initialize(ResourcePtr<IDXGIFactory7>& factory, D3D_FEATURE_LEVEL featureLevel, bool software);
};