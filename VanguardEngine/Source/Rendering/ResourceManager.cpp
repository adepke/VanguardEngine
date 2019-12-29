// Copyright (c) 2019 Andrew Depke

#include <Rendering/ResourceManager.h>
#include <Rendering/Device.h>
#include <Rendering/Resource.h>
#include <Utility/AlignedSize.h>

#include <D3D12MemAlloc.h>

#include <cstring>

ResourceManager::ResourceManager()
{
	constexpr auto UploadResourceSize = 1024 * 1024 * 64;

	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Alignment = 0;
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Width = AlignedSize(UploadResourceSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12MA::ALLOCATION_DESC AllocationDesc{};
	AllocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	AllocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;

	ID3D12Resource* RawResource = nullptr;
	D3D12MA::Allocation* AllocationHandle = nullptr;

	auto Result = Device.Allocator->CreateResource(&AllocationDesc, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &AllocationHandle, IID_PPV_ARGS(&RawResource));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to allocate write upload resource: " << Result;

		return;
	}

	D3D12_RANGE Range{ 0, 0 };

	Result = Result->GetResource()->Map(0, &Range, &UploadPtr);
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to map upload resource: " << Result;
	}

#if !BUILD_RELEASE
	Allocation->Resource->SetName(VGText("Upload Heap"));  // Set the name in the allocator.
	Result = Allocation->Resource->GetResource()->SetName(VGText("Upload Heap"));  // Set the name in the API.
	if (FAILED(Result))
	{
		VGLogWarning(Rendering) << "Failed to set upload resource name to: '" << Name << "':" << Result;
	}
#endif

	UploadResource = std::move(ResourcePtr<D3D12MA::Allocation>{ AllocationHandle });
}

std::shared_ptr<GPUBuffer> ResourceManager::Allocate(RenderDevice& Device, const ResourceDescription& Description, const std::wstring_view Name)
{
	VGScopedCPUStat("Resource Manager Allocate");

	const auto Alignment = Description.BindFlags & BindFlag::ConstantBuffer ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	const auto FinalSize = AlignedSize(Description.Size, static_cast<size_t>(Alignment));

	D3D12_RESOURCE_DESC ResourceDesc{};
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

	if (Description.BindFlags & BindFlag::RenderTarget)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	if (Description.BindFlags & BindFlag::DepthStencil)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}

	if (Description.BindFlags & BindFlag::UnorderedAccess)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	D3D12MA::ALLOCATION_DESC AllocationDesc{};
	AllocationDesc.HeapType = Description.UpdateRate == ResourceFrequency::Static ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
	AllocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;

	if (Description.BindFlags & BindFlag::RenderTarget)
	{
		// Render targets deserve their own partition. #TODO: Only apply this flag if the render target resolution is >=50% of the full screen resolution?
		AllocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
	}

	auto ResourceState = D3D12_RESOURCE_STATE_COMMON;
	
	if (Description.UpdateRate == ResourceFrequency::Dynamic)
	{
		ResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	else
	{
		// #TODO: Set the specific state based on the bind flags or leave it in common and require a resource barrier?
	}

	ID3D12Resource* RawResource = nullptr;
	D3D12MA::Allocation* AllocationHandle = nullptr;

	auto Result = Device.Allocator->CreateResource(&AllocationDesc, &ResourceDesc, ResourceState, nullptr, &AllocationHandle, IID_PPV_ARGS(&RawResource));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to allocate resource: " << Result;

		return nullptr;
	}

	// #TODO: Create a texture instead of a buffer if applicable.
	auto Allocation = std::make_shared<GPUBuffer>(ResourcePtr<D3D12MA::Allocation>{ AllocationHandle }, Description);

	// Name the resource.
	if (Device.Debugging)
	{
		Allocation->Resource->SetName(Name.data());  // Set the name in the allocator.
		const auto Result = Allocation->Resource->GetResource()->SetName(Name.data());  // Set the name in the API.
		if (FAILED(Result))
		{
			VGLogWarning(Rendering) << "Failed to set resource name to: '" << Name << "':" << Result;
		}
	}

	// Create view based on bind flags.

	if (Description.BindFlags & BindFlag::ConstantBuffer)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc{};
		ViewDesc.BufferLocation = Allocation->Resource->GetResource()->GetGPUVirtualAddress();
		ViewDesc.SizeInBytes = static_cast<UINT>(FinalSize);

		// #TODO: Device->CreateConstantBufferView
	}

	if (Description.BindFlags & BindFlag::ShaderResource)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
		ViewDesc.Buffer.FirstElement = 0;
		ViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

		// #TODO: Raw, structured, and typed buffer.
	}

	if (Description.BindFlags & BindFlag::UnorderedAccess)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC ViewDesc{};
		ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		// #TODO: Raw, structured, and typed buffer.
	}

	// #TODO: Render target view, etc.

	return std::move(Allocation);
}

void ResourceManager::Write(RenderDevice& Device, std::shared_ptr<GPUBuffer>& Buffer, std::vector<uint8_t>&& Source, size_t BufferOffset)
{
	VGScopedCPUStat("Resource Manager Write");

	if (Buffer->Description.UpdateRate == ResourceFrequency::Static)
	{
		VGScopedCPUStat("Write Static");

		std::memcpy(UploadAllocation.Data, Source.data(), Source.size());

		auto* TargetCommandList = static_cast<ID3D12GraphicsCommandList*>(Device.CopyCommandList[Device.Frame % RenderDevice::FrameCount].Get());
		TargetCommandList->CopyBufferRegion(Buffer->Resource->GetResource(), BufferOffset, UploadAllocation.Resource, UploadAllocation.Offset, Source.size());

		UploadOffset += Source.size();

		// #TODO: Resource barrier?

		CPUFrameResources[CurrentFrame % RenderDevice::FrameCount].push_back({ std::move(Resource), std::move(Source) });
	}

	else
	{
		VGScopedCPUStat("Write Dynamic");

		auto Result = Buffer->Resource->GetResource()->Map();
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Failed to map buffer resource during resource write: " << Result;

			return;
		}

		std::memcpy();

		Buffer->Resource->GetResource()->Unmap();
	}
}

void ResourceManager::CleanupFrameResources(size_t Frame)
{
	VGScopedCPUStat("Cleanup Frame Resources");

	const auto FrameID = Frame % RenderDevice::FrameCount;

	if (CPUFrameResources[FrameID].size() > 0)
	{
		VGLog(Rendering) << "Cleaning up " << CPUFrameResources[FrameID].size() << " CPU resources from frame " << Frame;
	}

	{
		VGScopedCPUStat("CPU Cleanup");

		CPUFrameResources[FrameID].clear();  // Cleans CPU resources.
	}

	{
		VGScopedCPUStat("GPU Cleanup");

		GPUFrameResources[FrameID].Reset();  // Cleans GPU resources.
	}

	++CurrentFrame;
}