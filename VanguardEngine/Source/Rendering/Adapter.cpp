// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/Adapter.h>

void Adapter::Initialize(ResourcePtr<IDXGIFactory7>& factory, D3D_FEATURE_LEVEL featureLevel, bool software)
{
	VGScopedCPUStat("Adapter Initialize");

	if (software)
	{
		factory->EnumWarpAdapter(IID_PPV_ARGS(adapterResource.Indirect()));
	}

	else
	{
		for (uint32_t i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapterResource.Indirect())) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 description;
			adapterResource->GetDesc1(&description);

			if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			// Check the device and feature level without creating it.
			const auto createDeviceResult = D3D12CreateDevice(adapterResource.Get(), featureLevel, __uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(createDeviceResult))
			{
				break;
			}
		}
	}

	VGEnsure(adapterResource, "Failed to find a suitable render adapter.");

	// #TODO: Adapter events.
	//adapterResource->RegisterVideoMemoryBudgetChangeNotificationEvent();
	//adapterResource->RegisterHardwareContentProtectionTeardownStatusEvent();

	DXGI_ADAPTER_DESC1 adapterDesc;
	adapterResource->GetDesc1(&adapterDesc);

	VGLog(Rendering) << "Using adapter: " << adapterDesc.Description;
}