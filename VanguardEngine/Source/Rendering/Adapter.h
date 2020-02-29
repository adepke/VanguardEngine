// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <Core/Windows/DirectX12Minimal.h>

class Adapter
{
private:
	ResourcePtr<IDXGIAdapter1> AdapterResource;

public:
	auto* Native() const noexcept { return AdapterResource.Get(); }

	void Initialize(ResourcePtr<IDXGIFactory7>& Factory, D3D_FEATURE_LEVEL FeatureLevel, bool Software);
};