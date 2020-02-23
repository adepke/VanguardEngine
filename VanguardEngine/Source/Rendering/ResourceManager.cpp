// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/ResourceManager.h>
#include <Rendering/Device.h>
#include <Rendering/Resource.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Utility/AlignedSize.h>

#include <D3D12MemAlloc.h>

#include <cstring>

void ResourceManager::CreateResourceViews(std::shared_ptr<Buffer>& Target)
{
	VGScopedCPUStat("Create Buffer Views");

	if (Target->Description.BindFlags & BindFlag::ConstantBuffer)
	{
		Target->CBV = Device->GetResourceHeap().Allocate();

		D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc{};
		ViewDesc.BufferLocation = Target->Native()->GetGPUVirtualAddress();
		ViewDesc.SizeInBytes = static_cast<UINT>(Target->Description.Size * Target->Description.Stride);

		Device->Native()->CreateConstantBufferView(&ViewDesc, *Target->CBV);
	}

	if (Target->Description.BindFlags & BindFlag::ShaderResource)
	{
		Target->SRV = Device->GetResourceHeap().Allocate();

		D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
		ViewDesc.Format = Target->Description.Format ? *Target->Description.Format : DXGI_FORMAT_UNKNOWN;  // Structured buffers don't have a format.
		ViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		ViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		ViewDesc.Buffer.FirstElement = 0;
		ViewDesc.Buffer.NumElements = static_cast<UINT>(Target->Description.Size);
		ViewDesc.Buffer.StructureByteStride = !Target->Description.Format || *Target->Description.Format == DXGI_FORMAT_UNKNOWN ? static_cast<UINT>(Target->Description.Stride) : 0;  // Structured buffers must have a stride.
		ViewDesc.Buffer.Flags = Target->Description.Format == DXGI_FORMAT_R32_TYPELESS ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;  // Byte address buffers (32 bit typeless) need the raw flag.

		Device->Native()->CreateShaderResourceView(Target->Native(), &ViewDesc, *Target->SRV);
	}

	if (Target->Description.BindFlags & BindFlag::UnorderedAccess)
	{
		Target->UAV = Device->GetResourceHeap().Allocate();

		D3D12_UNORDERED_ACCESS_VIEW_DESC ViewDesc{};
		ViewDesc.Format = Target->Description.Format ? *Target->Description.Format : DXGI_FORMAT_UNKNOWN;  // Structured buffers don't have a format.
		ViewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		ViewDesc.Buffer.FirstElement = 0;
		ViewDesc.Buffer.NumElements = static_cast<UINT>(Target->Description.Size);
		ViewDesc.Buffer.StructureByteStride = !Target->Description.Format || *Target->Description.Format == DXGI_FORMAT_UNKNOWN ? static_cast<UINT>(Target->Description.Stride) : 0;  // Structured buffers must have a stride.
		ViewDesc.Buffer.CounterOffsetInBytes = 0;  // #TODO: Counter offset.
		ViewDesc.Buffer.Flags = Target->Description.Format == DXGI_FORMAT_R32_TYPELESS ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;  // Byte address buffers (32 bit typeless) need the raw flag.

		if (Target->CounterBuffer && Target->Description.Format && Target->Description.Format != DXGI_FORMAT_UNKNOWN)
		{
			VGLogWarning(Rendering) << "Buffer format must be unknown if using a counter buffer.";
			ViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		}

		ID3D12Resource* CounterBuffer = nullptr;
		if (Target->CounterBuffer)
		{
			CounterBuffer = (*Target->CounterBuffer)->GetResource();
		}

		Device->Native()->CreateUnorderedAccessView(Target->Native(), CounterBuffer, &ViewDesc, *Target->UAV);
	}
}

void ResourceManager::CreateResourceViews(std::shared_ptr<Texture>& Target)
{
	VGScopedCPUStat("Create Texture Views");

	if (Target->Description.BindFlags & BindFlag::RenderTarget)
	{
		Target->RTV = Device->GetRenderTargetHeap().Allocate();

		D3D12_RENDER_TARGET_VIEW_DESC ViewDesc{};
		ViewDesc.Format = Target->Description.Format;
		switch (Target->Native()->GetDesc().Dimension)  // #TODO: Support texture arrays and multi-sample textures.
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			ViewDesc.Texture1D.MipSlice = 0;  // #TODO: Support texture mips.
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			ViewDesc.Texture2D.MipSlice = 0;
			ViewDesc.Texture2D.PlaneSlice = 0;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			ViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			ViewDesc.Texture3D.MipSlice = 0;
			ViewDesc.Texture3D.FirstWSlice = 0;
			ViewDesc.Texture3D.WSize = -1;
			break;
		}

		Device->Native()->CreateRenderTargetView(Target->Native(), &ViewDesc, *Target->RTV);
	}

	if (Target->Description.BindFlags & BindFlag::DepthStencil)
	{
		Target->DSV = Device->GetDepthStencilHeap().Allocate();

		D3D12_DEPTH_STENCIL_VIEW_DESC ViewDesc{};
		ViewDesc.Format = Target->Description.Format;
		switch (Target->Native()->GetDesc().Dimension)  // #TODO: Support texture arrays and multi-sample textures.
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			ViewDesc.Texture1D.MipSlice = 0;  // #TODO: Support texture mips.
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			ViewDesc.Texture2D.MipSlice = 0;
			break;
		}
		ViewDesc.Flags = D3D12_DSV_FLAG_NONE;  // #TODO: Depth stencil flags.

		Device->Native()->CreateDepthStencilView(Target->Native(), &ViewDesc, *Target->DSV);
	}

	if (Target->Description.BindFlags & BindFlag::ShaderResource)
	{
		Target->SRV = Device->GetResourceHeap().Allocate();

		D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc{};
		ViewDesc.Format = Target->Description.Format;
		switch (Target->Native()->GetDesc().Dimension)  // #TODO: Support texture arrays and multi-sample textures.
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			ViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			ViewDesc.Texture1D.MostDetailedMip = 0;
			ViewDesc.Texture2D.MipLevels = -1;  // #TODO: Support texture mips.
			ViewDesc.Texture2D.ResourceMinLODClamp = 0.f;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			ViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			ViewDesc.Texture2D.MostDetailedMip = 0;
			ViewDesc.Texture2D.MipLevels = -1;
			ViewDesc.Texture2D.PlaneSlice = 0;
			ViewDesc.Texture2D.ResourceMinLODClamp = 0.f;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			ViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			ViewDesc.Texture3D.MostDetailedMip = 0;
			ViewDesc.Texture3D.MipLevels = -1;
			ViewDesc.Texture3D.ResourceMinLODClamp = 0.f;
			break;
		}
		ViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		Device->Native()->CreateShaderResourceView(Target->Native(), &ViewDesc, *Target->SRV);
	}

	if (Target->Description.BindFlags & BindFlag::UnorderedAccess)
	{
		// #TODO: Texture UAVs.
	}
}

void ResourceManager::NameResource(ResourcePtr<D3D12MA::Allocation>& Target, const std::wstring_view Name)
{
#if !BUILD_RELEASE
	if (Device->Debugging)
	{
		Target->SetName(Name.data());  // Set the name in the allocator.
		const auto Result = Target->GetResource()->SetName(Name.data());  // Set the name in the API.
		if (FAILED(Result))
		{
			VGLogWarning(Rendering) << "Failed to set resource name to: '" << Name << "': " << Result;
		}
	}
#endif
}

void ResourceManager::Initialize(RenderDevice* InDevice, size_t BufferedFrames)
{
	VGScopedCPUStat("Resource Manager Initialize");

	Device = InDevice;
	FrameCount = BufferedFrames;

	UploadResources.resize(FrameCount);
	UploadOffsets.resize(FrameCount);
	UploadPtrs.resize(FrameCount);

	constexpr auto UploadResourceSize = 1024 * 1024 * 64;

	for (uint32_t Index = 0; Index < FrameCount; ++Index)
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

		auto Result = Device->Allocator->CreateResource(&AllocationDesc, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &AllocationHandle, IID_PPV_ARGS(&RawResource));
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Failed to allocate write upload resource: " << Result;

			continue;
		}

		D3D12_RANGE Range{ 0, 0 };

		Result = AllocationHandle->GetResource()->Map(0, &Range, &UploadPtrs[Index]);
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Failed to map upload resource: " << Result;

			continue;
		}

		UploadResources[Index] = std::move(ResourcePtr<D3D12MA::Allocation>{ AllocationHandle });

		NameResource(UploadResources[Index], VGText("Upload Heap"));
	}
}

std::shared_ptr<Buffer> ResourceManager::AllocateBuffer(const BufferDescription& Description, const std::wstring_view Name)
{
	VGScopedCPUStat("Allocate Buffer");

	const auto Alignment = Description.BindFlags & BindFlag::ConstantBuffer ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT : D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	const auto FinalSize = AlignedSize(Description.Size, static_cast<size_t>(Alignment));

	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Alignment = 0;
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Width = FinalSize;
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.Format = Description.Format ? *Description.Format : DXGI_FORMAT_UNKNOWN;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (Description.BindFlags & BindFlag::UnorderedAccess)
	{
		ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	D3D12MA::ALLOCATION_DESC AllocationDesc{};
	AllocationDesc.HeapType = Description.UpdateRate == ResourceFrequency::Static ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
	AllocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;

	auto ResourceState = Description.InitialState;

	if (Description.UpdateRate == ResourceFrequency::Dynamic)
	{
		ResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	ID3D12Resource* RawResource = nullptr;
	D3D12MA::Allocation* AllocationHandle = nullptr;

	auto Result = Device->Allocator->CreateResource(&AllocationDesc, &ResourceDesc, ResourceState, nullptr, &AllocationHandle, IID_PPV_ARGS(&RawResource));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to allocate buffer: " << Result;

		return nullptr;
	}

	auto Allocation = std::make_shared<Buffer>();
	Allocation->Description = Description;
	Allocation->Allocation = std::move(ResourcePtr<D3D12MA::Allocation>{ AllocationHandle });
	Allocation->State = ResourceState;

	CreateResourceViews(Allocation);
	NameResource(Allocation->Allocation, Name);

	return std::move(Allocation);
}

std::shared_ptr<Texture> ResourceManager::AllocateTexture(const TextureDescription& Description, const std::wstring_view Name)
{
	VGScopedCPUStat("Allocate Texture");

	// Early validation.
	VGAssert(Description.UpdateRate != ResourceFrequency::Dynamic, "Failed to create texture, cannot have dynamic update rate.");

	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Alignment = 0;
	ResourceDesc.Dimension = Description.Height > 1 ? (Description.Depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D) : D3D12_RESOURCE_DIMENSION_TEXTURE1D;
	ResourceDesc.Width = Description.Width;
	ResourceDesc.Height = Description.Height;
	ResourceDesc.DepthOrArraySize = Description.Depth;
	ResourceDesc.Format = Description.Format;
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
		if (Description.Depth > 0)
		{
			VGLogWarning(Rendering) << "3D textures cannot have depth stencil binding.";
		}

		else
		{
			ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			if ((Description.BindFlags & BindFlag::ShaderResource) == 0)
			{
				ResourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
			}
		}
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

	auto ResourceState = Description.InitialState;

	if (Description.UpdateRate == ResourceFrequency::Dynamic)
	{
		ResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	ID3D12Resource* RawResource = nullptr;
	D3D12MA::Allocation* AllocationHandle = nullptr;

	auto Result = Device->Allocator->CreateResource(&AllocationDesc, &ResourceDesc, ResourceState, nullptr, &AllocationHandle, IID_PPV_ARGS(&RawResource));
	if (FAILED(Result))
	{
		VGLogError(Rendering) << "Failed to allocate texture: " << Result;

		return nullptr;
	}

	auto Allocation = std::make_shared<Texture>();
	Allocation->Description = Description;
	Allocation->Allocation = std::move(ResourcePtr<D3D12MA::Allocation>{ AllocationHandle });
	Allocation->State = ResourceState;

	CreateResourceViews(Allocation);
	NameResource(Allocation->Allocation, Name);

	return std::move(Allocation);
}

std::shared_ptr<Texture> ResourceManager::TextureFromSwapChain(void* Surface, const std::wstring_view Name)
{
	VGScopedCPUStat("Texture From Swap Chain");

	TextureDescription Description{};
	Description.UpdateRate = ResourceFrequency::Static;
	Description.BindFlags = BindFlag::RenderTarget;
	Description.AccessFlags = AccessFlag::GPUWrite;
	Description.InitialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	Description.Width = static_cast<uint32_t>(Device->RenderWidth);
	Description.Height = static_cast<uint32_t>(Device->RenderHeight);
	Description.Depth = 1;
	Description.Format = /*DXGI_FORMAT_R10G10B10A2_UNORM*/ DXGI_FORMAT_B8G8R8A8_UNORM;

	auto Allocation = std::make_shared<Texture>();
	Allocation->Description = Description;
	Allocation->Allocation = std::move(ResourcePtr<D3D12MA::Allocation>{ new D3D12MA::Allocation });
	Allocation->Allocation->CreateManual(static_cast<ID3D12Resource*>(Surface));
	Allocation->State = Description.InitialState;

	CreateResourceViews(Allocation);
	NameResource(Allocation->Allocation, Name);

	return std::move(Allocation);
}

void ResourceManager::WriteBuffer(std::shared_ptr<Buffer>& Target, const std::vector<uint8_t>& Source, size_t TargetOffset)
{
	if (Target->Description.UpdateRate == ResourceFrequency::Static)
	{
		VGScopedCPUStat("Buffer Write Static");

		VGAssert(Target->Description.AccessFlags & AccessFlag::CPUWrite, "Failed to write to static buffer, no CPU write access.");
		VGAssert(Target->Description.Size + TargetOffset <= Source.size(), "Failed to write to static buffer, source buffer is larger than target.");

		const auto FrameIndex = Device->GetFrameIndex();

		VGAssert(UploadOffsets[FrameIndex] + Source.size() <= UploadResources[FrameIndex]->GetResource()->GetDesc().Width, "Failed to write to static buffer, exhausted frame upload heap.");

		std::memcpy(static_cast<uint8_t*>(UploadPtrs[FrameIndex]) + UploadOffsets[FrameIndex], Source.data(), Source.size());

		// #TODO: Resource barrier?

		auto* TargetCommandList = Device->GetCopyList().Native();
		TargetCommandList->CopyBufferRegion(Target->Native(), TargetOffset, UploadResources[FrameIndex]->GetResource(), UploadOffsets[FrameIndex], Source.size());  // #TODO: If we have offsets of 0, use CopyResource.

		// #TODO: Resource barrier?

		UploadOffsets[FrameIndex] += Source.size();
	}

	else
	{
		VGScopedCPUStat("Buffer Write Dynamic");

		VGAssert(Target->Description.AccessFlags & AccessFlag::CPUWrite, "Failed to write to dynamic buffer, no CPU write access.");
		VGAssert(Target->Description.Size + TargetOffset >= Source.size(), "Failed to write to dynamic buffer, source is larger than target.");

		void* MappedPtr = nullptr;

		D3D12_RANGE MapRange{ TargetOffset, TargetOffset + Source.size() };

		auto Result = Target->Native()->Map(0, &MapRange, &MappedPtr);
		if (FAILED(Result))
		{
			VGLogError(Rendering) << "Failed to map buffer resource during resource write: " << Result;

			return;
		}

		std::memcpy(MappedPtr, Source.data(), Source.size());

		Target->Native()->Unmap(0, nullptr);
	}
}

void ResourceManager::WriteTexture(std::shared_ptr<Texture>& Target, const std::vector<uint8_t>& Source)
{
	VGScopedCPUStat("Texture Write");

	VGAssert(Target->Description.AccessFlags & AccessFlag::CPUWrite, "Failed to write to texture, no CPU write access.");
	VGAssert(Target->Description.Width * Target->Description.Height * Target->Description.Depth <= Source.size(), "Failed to write to texture, source is larger than target.");

	const auto FrameIndex = Device->GetFrameIndex();

	VGAssert(UploadOffsets[FrameIndex] + Source.size() <= UploadResources[FrameIndex]->GetResource()->GetDesc().Width, "Failed to write to texture, exhausted frame upload heap.");

	std::memcpy(static_cast<uint8_t*>(UploadPtrs[FrameIndex]) + UploadOffsets[FrameIndex], Source.data(), Source.size());

	D3D12_TEXTURE_COPY_LOCATION SourceCopyDesc{};
	SourceCopyDesc.pResource = UploadResources[FrameIndex]->GetResource();
	SourceCopyDesc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	SourceCopyDesc.PlacedFootprint.Offset = UploadOffsets[FrameIndex];
	SourceCopyDesc.PlacedFootprint.Footprint.Format = Target->Description.Format;
	SourceCopyDesc.PlacedFootprint.Footprint.Width = Target->Description.Width;  // #TODO: Support custom copy sizes.
	SourceCopyDesc.PlacedFootprint.Footprint.Height = Target->Description.Height;
	SourceCopyDesc.PlacedFootprint.Footprint.Depth = Target->Description.Depth;
	SourceCopyDesc.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * 256;  // #TODO: Properly determine this number from the source data?

	D3D12_TEXTURE_COPY_LOCATION TargetCopyDesc{};
	TargetCopyDesc.pResource = Target->Native();
	TargetCopyDesc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	TargetCopyDesc.SubresourceIndex = 0;

	// #TODO: Support custom copy sizes.
	D3D12_BOX SourceBox{};
	SourceBox.left = 0;
	SourceBox.top = 0;
	SourceBox.front = 0;
	SourceBox.right = Target->Description.Width;
	SourceBox.bottom = Target->Description.Height;
	SourceBox.back = Target->Description.Depth;

	// #TODO: Resource barrier?

	auto* TargetCommandList = Device->GetCopyList().Native();
	TargetCommandList->CopyTextureRegion(&TargetCopyDesc, 0, 0, 0, &SourceCopyDesc, &SourceBox);  // #TODO: If we have offsets of 0, use CopyResource.

	// #TODO: Resource barrier?

	UploadOffsets[FrameIndex] += Source.size();
}

void ResourceManager::CleanupFrameResources(size_t Frame)
{
	VGScopedCPUStat("Cleanup Frame Resources");

	UploadOffsets[Frame % FrameCount] = 0;
}