// Copyright (c) 2019 Andrew Depke

#include <Rendering/ResourceManager.h>
#include <Rendering/Resource.h>
#include <Utility/AlignedSize.h>

#include <D3D12MemAlloc.h>

std::shared_ptr<GPUBuffer> ResourceManager::Allocate(ResourcePtr<D3D12MA::Allocator>& Allocator, const ResourceDescription& Description)
{
	const auto Alignment = Description.BindFlags & BindFlag::BindConstantBuffer ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	const auto FinalSize = AlignedSize(Description.Size, static_cast<size_t>(Alignment));

	D3D12_RESOURCE_DESC ResourceDesc;
	ZeroMemory(&ResourceDesc, sizeof(ResourceDesc));
	ResourceDesc.Alignment = 0;
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Width = FinalSize;
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (Description.BindFlags & BindFlag::BindRenderTarget)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	if (Description.BindFlags & BindFlag::BindDepthStencil)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}

	if (Description.BindFlags & BindFlag::BindUnorderedAccess)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	D3D12MA::ALLOCATION_DESC AllocationDesc;
	AllocationDesc.HeapType = Description.UpdateRate == ResourceFrequency::Static ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
	AllocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;

	if (Description.BindFlags & BindFlag::BindRenderTarget)
	{
		// Render targets deserve their own partition. #TODO: Only apply this flag if the render target resolution is >=50% of the full screen resolution?
		AllocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
	}

	ID3D12Resource* RawResource = nullptr;
	D3D12MA::Allocation* AllocationHandle = nullptr;

	auto Result = Allocator->CreateResource(&AllocationDesc, &ResourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, &AllocationHandle, IID_PPV_ARGS(&RawResource));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to allocate resource: " << Result;

		return nullptr;
	}

	auto Allocation = std::make_shared<GPUBuffer>(ResourcePtr<D3D12MA::Allocation>{ AllocationHandle }, FinalSize);

	// Create view based on bind flags.

	if (Description.BindFlags & BindFlag::BindConstantBuffer)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc{};
		ViewDesc.BufferLocation = Allocation->Resource->GetResource()->GetGPUVirtualAddress();
		ViewDesc.SizeInBytes = static_cast<UINT>(Allocation->Size);

		// #TODO: Device->CreateConstantBufferView
	}

	else if (Description.BindFlags & BindFlag::BindShaderResource)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
		ViewDesc.Buffer.FirstElement = 0;
		ViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

		// #TODO: Raw, structured, and typed buffer.
	}

	else if (Description.BindFlags & BindFlag::BindUnorderedAccess)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC ViewDesc{};
		ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		// #TODO: Raw, structured, and typed buffer.
	}

	// #TODO: Render target view, etc.

	return std::move(Allocation);
}