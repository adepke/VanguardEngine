// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/Adapter.h>

void Adapter::Initialize(ResourcePtr<IDXGIFactory7>& Factory, D3D_FEATURE_LEVEL FeatureLevel, bool Software)
{
	VGScopedCPUStat("Adapter Initialize");

	if (Software)
	{
		Factory->EnumWarpAdapter(IID_PPV_ARGS(AdapterResource.Indirect()));
	}

	else
	{
		for (uint32_t Index = 0; Factory->EnumAdapterByGpuPreference(Index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(AdapterResource.Indirect())) != DXGI_ERROR_NOT_FOUND; ++Index)
		{
			DXGI_ADAPTER_DESC1 Description;
			AdapterResource->GetDesc1(&Description);

			if ((Description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			// Check the device and feature level without creating it.
			const auto CreateDeviceResult = D3D12CreateDevice(AdapterResource.Get(), FeatureLevel, __uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(CreateDeviceResult))
			{
				break;
			}
		}
	}

	VGEnsure(AdapterResource, "Failed to find a suitable render adapter.");

	// #TODO: Adapter events.
	//AdapterResource->RegisterVideoMemoryBudgetChangeNotificationEvent();
	//AdapterResource->RegisterHardwareContentProtectionTeardownStatusEvent();

	DXGI_ADAPTER_DESC1 AdapterDesc;
	AdapterResource->GetDesc1(&AdapterDesc);

	VGLog(Rendering) << "Using adapter: " << AdapterDesc.Description;
}