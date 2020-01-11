// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/ResourceManager.h>
#include <Rendering/Device.h>
#include <Rendering/Resource.h>
#include <Utility/AlignedSize.h>

#include <D3D12MemAlloc.h>

#include <cstring>

void ResourceManager::CreateBindings(const std::shared_ptr<GPUBuffer>& Buffer, const ResourceDescription& Description)
{
	// Create view based on bind flags.

	if (Description.BindFlags & BindFlag::ConstantBuffer)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc{};
		ViewDesc.BufferLocation = Buffer->Resource->GetResource()->GetGPUVirtualAddress();
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
}

void ResourceManager::Initialize(RenderDevice& Device, size_t BufferedFrames)
{
	VGScopedCPUStat("Resource Manager Initialize");

	FrameCount = BufferedFrames;

	UploadResources.resize(FrameCount);
	UploadOffsets.resize(FrameCount);
	UploadPtrs.resize(FrameCount);

	constexpr auto UploadResourceSize = 1024 * 1024 * 64;

	for (auto Index = 0; Index < FrameCount; ++Index)
	{
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

		Result = AllocationHandle->GetResource()->Map(0, &Range, &UploadPtrs[Index]);
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Failed to map upload resource: " << Result;
		}

#if !BUILD_RELEASE
		constexpr auto* Name = VGText("Upload Heap");
		AllocationHandle->SetName(Name);  // Set the name in the allocator.
		Result = AllocationHandle->GetResource()->SetName(Name);  // Set the name in the API.
		if (FAILED(Result))
		{
			VGLogWarning(Rendering) << "Failed to set upload resource name to: '" << Name << "':" << Result;
		}
#endif

		UploadResources[Index] = std::move(ResourcePtr<D3D12MA::Allocation>{ AllocationHandle });
	}
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

	CreateBindings(Description);

	return std::move(Allocation);
}

std::shared_ptr<GPUBuffer> ResourceManager::AllocateFromAPIBuffer(const ResourceDescription& Description, void* Buffer, const std::wstring_view Name)
{
	// #TEMP
	return {};

	/*
	D3D12MA::Allocation ManualAllocationHandle;

	// #TODO: Create a texture instead of a buffer if applicable.
	auto Allocation = std::make_shared<GPUBuffer>(ResourcePtr<D3D12MA::Allocation>{ AllocationHandle }, Description);

	return std::move(Allocation);
	*/
}

void ResourceManager::Write(RenderDevice& Device, std::shared_ptr<GPUBuffer>& Buffer, const std::vector<uint8_t>& Source, size_t BufferOffset)
{
	VGScopedCPUStat("Resource Manager Write");

	if (Buffer->Description.UpdateRate == ResourceFrequency::Static)
	{
		VGScopedCPUStat("Write Static");

		const auto FrameIndex = Device.GetFrameIndex();

		std::memcpy(static_cast<uint8_t*>(UploadPtrs[FrameIndex]) + UploadOffsets[FrameIndex], Source.data(), Source.size());

		// #TODO: Resource barrier?

		auto* TargetCommandList = Device.GetCopyList()->Native();
		TargetCommandList->CopyBufferRegion(Buffer->Resource->GetResource(), BufferOffset, UploadResources[FrameIndex]->GetResource(), UploadOffsets[FrameID], Source.size());

		// #TODO: Resource barrier?

		UploadOffsets[FrameIndex] += Source.size();
	}

	else
	{
		VGScopedCPUStat("Write Dynamic");

		void* MappedPtr = nullptr;

		D3D12_RANGE MapRange{ BufferOffset, BufferOffset + Source.size() };

		auto Result = Buffer->Resource->GetResource()->Map(0, &MapRange, &MappedPtr);
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Failed to map buffer resource during resource write: " << Result;

			return;
		}

		std::memcpy(MappedPtr, Source.data(), Source.size());

		D3D12_RANGE UnmapRange{ 0, 0 };

		Buffer->Resource->GetResource()->Unmap(0, &UnmapRange);
	}
}

void ResourceManager::CleanupFrameResources(size_t Frame)
{
	VGScopedCPUStat("Cleanup Frame Resources");

	UploadOffsets[Frame % FrameCount] = 0;
	++CurrentFrame;
}