// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/ResourceManager.h>
#include <Rendering/Device.h>
#include <Rendering/Resource.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Utility/AlignedSize.h>

#include <D3D12MemAlloc.h>

#include <cstring>

void ResourceManager::CreateResourceViews(std::shared_ptr<Buffer>& target)
{
	VGScopedCPUStat("Create Buffer Views");

	if (target->description.bindFlags & BindFlag::ConstantBuffer)
	{
		target->CBV = device->AllocateDescriptor(DescriptorType::Default);

		D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
		viewDesc.BufferLocation = target->Native()->GetGPUVirtualAddress();
		viewDesc.SizeInBytes = static_cast<UINT>(AlignedSize(target->description.size * target->description.stride, 256));  // CBV's must be a multiple of 256 bytes.

		device->Native()->CreateConstantBufferView(&viewDesc, *target->CBV);
	}

	if (target->description.bindFlags & BindFlag::ShaderResource)
	{
		target->SRV = device->AllocateDescriptor(DescriptorType::Default);

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = target->description.format ? *target->description.format : DXGI_FORMAT_UNKNOWN;  // Structured buffers don't have a format.
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Buffer.FirstElement = 0;
		viewDesc.Buffer.NumElements = static_cast<UINT>(target->description.size);
		viewDesc.Buffer.StructureByteStride = !target->description.format || *target->description.format == DXGI_FORMAT_UNKNOWN ? static_cast<UINT>(target->description.stride) : 0;  // Structured buffers must have a stride.
		viewDesc.Buffer.Flags = target->description.format == DXGI_FORMAT_R32_TYPELESS ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;  // Byte address buffers (32 bit typeless) need the raw flag.

		device->Native()->CreateShaderResourceView(target->Native(), &viewDesc, *target->SRV);
	}

	if (target->description.bindFlags & BindFlag::UnorderedAccess)
	{
		target->UAV = device->AllocateDescriptor(DescriptorType::Default);

		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = target->description.format ? *target->description.format : DXGI_FORMAT_UNKNOWN;  // Structured buffers don't have a format.
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		viewDesc.Buffer.FirstElement = 0;
		viewDesc.Buffer.NumElements = static_cast<UINT>(target->description.size);
		viewDesc.Buffer.StructureByteStride = !target->description.format || *target->description.format == DXGI_FORMAT_UNKNOWN ? static_cast<UINT>(target->description.stride) : 0;  // Structured buffers must have a stride.
		viewDesc.Buffer.CounterOffsetInBytes = 0;  // #TODO: Counter offset.
		viewDesc.Buffer.Flags = target->description.format == DXGI_FORMAT_R32_TYPELESS ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;  // Byte address buffers (32 bit typeless) need the raw flag.

		if (target->counterBuffer && target->description.format && target->description.format != DXGI_FORMAT_UNKNOWN)
		{
			VGLogWarning(Rendering) << "Buffer format must be unknown if using a counter buffer.";
			viewDesc.Format = DXGI_FORMAT_UNKNOWN;
		}

		ID3D12Resource* counterBuffer = nullptr;
		if (target->counterBuffer)
		{
			counterBuffer = (*target->counterBuffer)->GetResource();
		}

		device->Native()->CreateUnorderedAccessView(target->Native(), counterBuffer, &viewDesc, *target->UAV);
	}
}

void ResourceManager::CreateResourceViews(std::shared_ptr<Texture>& target)
{
	VGScopedCPUStat("Create Texture Views");

	if (target->description.bindFlags & BindFlag::RenderTarget)
	{
		target->RTV = device->AllocateDescriptor(DescriptorType::RenderTarget);

		D3D12_RENDER_TARGET_VIEW_DESC viewDesc{};
		viewDesc.Format = target->description.format;
		switch (target->Native()->GetDesc().Dimension)  // #TODO: Support texture arrays and multi-sample textures.
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MipSlice = 0;  // #TODO: Support texture mips.
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipSlice = 0;
			viewDesc.Texture2D.PlaneSlice = 0;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MipSlice = 0;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = -1;
			break;
		default:
			VGLogError(Rendering) << "Render target views for textures in " << target->Native()->GetDesc().Dimension << " dimension is unsupported.";
		}

		device->Native()->CreateRenderTargetView(target->Native(), &viewDesc, *target->RTV);
	}

	if (target->description.bindFlags & BindFlag::DepthStencil)
	{
		target->DSV = device->AllocateDescriptor(DescriptorType::DepthStencil);

		D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
		viewDesc.Format = target->description.format;
		switch (target->Native()->GetDesc().Dimension)  // #TODO: Support texture arrays and multi-sample textures.
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MipSlice = 0;  // #TODO: Support texture mips.
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipSlice = 0;
			break;
		default:
			VGLogError(Rendering) << "Depth stencil views for textures in " << target->Native()->GetDesc().Dimension << " dimension is unsupported.";
		}
		viewDesc.Flags = D3D12_DSV_FLAG_NONE;  // #TODO: Depth stencil flags.

		device->Native()->CreateDepthStencilView(target->Native(), &viewDesc, *target->DSV);
	}

	if (target->description.bindFlags & BindFlag::ShaderResource)
	{
		target->SRV = device->AllocateDescriptor(DescriptorType::Default);

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = target->description.format;
		switch (target->Native()->GetDesc().Dimension)  // #TODO: Support texture arrays and multi-sample textures.
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MostDetailedMip = 0;
			viewDesc.Texture1D.MipLevels = -1;  // #TODO: Support texture mips.
			viewDesc.Texture1D.ResourceMinLODClamp = 0.f;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MostDetailedMip = 0;
			viewDesc.Texture2D.MipLevels = -1;
			viewDesc.Texture2D.PlaneSlice = 0;
			viewDesc.Texture2D.ResourceMinLODClamp = 0.f;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MostDetailedMip = 0;
			viewDesc.Texture3D.MipLevels = -1;
			viewDesc.Texture3D.ResourceMinLODClamp = 0.f;
			break;
		default:
			VGLogError(Rendering) << "Shader resource views for textures in " << target->Native()->GetDesc().Dimension << " dimension is unsupported.";
		}
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		device->Native()->CreateShaderResourceView(target->Native(), &viewDesc, *target->SRV);
	}

	if (target->description.bindFlags & BindFlag::UnorderedAccess)
	{
		// #TODO: Texture UAVs.
	}
}

void ResourceManager::NameResource(ResourcePtr<D3D12MA::Allocation>& target, const std::wstring_view name)
{
#if !BUILD_RELEASE
	target->SetName(name.data());  // Set the name in the allocator.
	const auto result = target->GetResource()->SetName(name.data());  // Set the name in the API.
	if (FAILED(result))
	{
		VGLogWarning(Rendering) << "Failed to set resource name to: '" << name << "': " << result;
	}
#endif
}

void ResourceManager::Initialize(RenderDevice* inDevice, size_t bufferedFrames)
{
	VGScopedCPUStat("Resource Manager Initialize");

	device = inDevice;
	frameCount = bufferedFrames;

	uploadResources.resize(frameCount);
	uploadOffsets.resize(frameCount);
	uploadPtrs.resize(frameCount);

	constexpr auto uploadResourceSize = 1024 * 1024 * 512;

	for (uint32_t i = 0; i < frameCount; ++i)
	{
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = uploadResourceSize;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;  // Buffers are always row major.
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;

		ID3D12Resource* rawResource = nullptr;
		D3D12MA::Allocation* allocationHandle = nullptr;

		// Upload heap resources must always be in generic read state.
		auto result = device->allocator->CreateResource(&allocationDesc, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &allocationHandle, IID_PPV_ARGS(&rawResource));
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Failed to allocate write upload resource: " << result;

			continue;
		}

		D3D12_RANGE range{ 0, 0 };

		result = allocationHandle->GetResource()->Map(0, &range, &uploadPtrs[i]);
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Failed to map upload resource: " << result;

			continue;
		}

		uploadResources[i] = std::move(ResourcePtr<D3D12MA::Allocation>{ allocationHandle });

		NameResource(uploadResources[i], VGText("Upload Heap"));
	}
}

std::shared_ptr<Buffer> ResourceManager::AllocateBuffer(const BufferDescription& description, const std::wstring_view name)
{
	VGScopedCPUStat("Allocate Buffer");

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = description.size * description.stride;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = description.format ? *description.format : DXGI_FORMAT_UNKNOWN;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (description.bindFlags & BindFlag::UnorderedAccess)
	{
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	D3D12MA::ALLOCATION_DESC allocationDesc{};
	allocationDesc.HeapType = description.updateRate == ResourceFrequency::Static ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
	allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;

	// #TODO: Do we need these flags if we specify vertex/constant buffer or index buffer?
	auto resourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	if (description.updateRate == ResourceFrequency::Dynamic)
	{
		resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	else
	{
		if (description.bindFlags & BindFlag::ConstantBuffer) resourceState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		else if (description.bindFlags & BindFlag::VertexBuffer) resourceState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (description.bindFlags & BindFlag::IndexBuffer) resourceState |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	}

	ID3D12Resource* rawResource = nullptr;
	D3D12MA::Allocation* allocationHandle = nullptr;

	auto result = device->allocator->CreateResource(&allocationDesc, &resourceDesc, resourceState, nullptr, &allocationHandle, IID_PPV_ARGS(&rawResource));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to allocate buffer: " << result;

		return nullptr;
	}

	auto allocation = std::make_shared<Buffer>();
	allocation->description = description;
	allocation->allocation = std::move(ResourcePtr<D3D12MA::Allocation>{ allocationHandle });
	allocation->state = resourceState;

	CreateResourceViews(allocation);
	NameResource(allocation->allocation, name);

	return std::move(allocation);
}

std::shared_ptr<Texture> ResourceManager::AllocateTexture(const TextureDescription& description, const std::wstring_view name)
{
	VGScopedCPUStat("Allocate Texture");

	// Early validation.
	VGAssert(description.updateRate != ResourceFrequency::Dynamic, "Failed to create texture, cannot have dynamic update rate.");

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Dimension = description.height > 1 ? (description.depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D) : D3D12_RESOURCE_DIMENSION_TEXTURE1D;
	resourceDesc.Width = description.width;
	resourceDesc.Height = description.height;
	resourceDesc.DepthOrArraySize = description.depth;
	resourceDesc.Format = description.format;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;  // Prefer to let the adapter choose the most efficient layout. See: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_texture_layout
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (description.bindFlags & BindFlag::RenderTarget)
	{
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	if (description.bindFlags & BindFlag::DepthStencil)
	{
		if (description.depth > 1)
		{
			VGLogWarning(Rendering) << "3D textures cannot have depth stencil binding.";
		}

		else
		{
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			if ((description.bindFlags & BindFlag::ShaderResource) == 0)
			{
				resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
			}
		}
	}

	if (description.bindFlags & BindFlag::UnorderedAccess)
	{
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	D3D12MA::ALLOCATION_DESC allocationDesc{};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_NONE;

	if (description.bindFlags & BindFlag::RenderTarget)
	{
		// Render targets deserve their own partition. #TODO: Only apply this flag if the render target resolution is >=50% of the full screen resolution?
		allocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
	}

	auto resourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	if (description.bindFlags & BindFlag::DepthStencil)
	{
		// Depth stencil textures cannot be in standard shader resource format if we don't have an SRV binding. Guess the initial state to try and avoid an immediate transition.
		resourceState = (description.accessFlags & AccessFlag::GPUWrite) ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_DEPTH_READ;
	}

	ID3D12Resource* rawResource = nullptr;
	D3D12MA::Allocation* allocationHandle = nullptr;

	D3D12_CLEAR_VALUE clearValue{};
	bool useClearValue = false;

	if (description.bindFlags & BindFlag::RenderTarget)
	{
		useClearValue = true;

		clearValue.Format = description.format;
		clearValue.Color[0] = 0.f;
		clearValue.Color[1] = 0.f;
		clearValue.Color[2] = 0.f;
		clearValue.Color[3] = 1.f;
	}

	else if (description.bindFlags & BindFlag::DepthStencil)
	{
		useClearValue = true;

		clearValue.Format = description.format;
		clearValue.DepthStencil.Depth = 1.f;
		clearValue.DepthStencil.Stencil = 0;
	}

	auto result = device->allocator->CreateResource(&allocationDesc, &resourceDesc, resourceState, useClearValue ? &clearValue : nullptr, &allocationHandle, IID_PPV_ARGS(&rawResource));
	if (FAILED(result))
	{
		VGLogError(Rendering) << "Failed to allocate texture: " << result;

		return nullptr;
	}

	auto allocation = std::make_shared<Texture>();
	allocation->description = description;
	allocation->allocation = std::move(ResourcePtr<D3D12MA::Allocation>{ allocationHandle });
	allocation->state = resourceState;

	CreateResourceViews(allocation);
	NameResource(allocation->allocation, name);

	return std::move(allocation);
}

std::shared_ptr<Texture> ResourceManager::TextureFromSwapChain(void* surface, const std::wstring_view name)
{
	VGScopedCPUStat("Texture From Swap Chain");

	TextureDescription description{};
	description.updateRate = ResourceFrequency::Static;
	description.bindFlags = BindFlag::RenderTarget;
	description.accessFlags = AccessFlag::GPUWrite;
	description.width = device->renderWidth;
	description.height = device->renderHeight;
	description.depth = 1;
	description.format = /*DXGI_FORMAT_R10G10B10A2_UNORM*/ DXGI_FORMAT_B8G8R8A8_UNORM;

	auto allocation = std::make_shared<Texture>();
	allocation->description = description;
	allocation->allocation = std::move(ResourcePtr<D3D12MA::Allocation>{ new D3D12MA::Allocation });
	allocation->allocation->CreateManual(static_cast<ID3D12Resource*>(surface));
	allocation->state = D3D12_RESOURCE_STATE_COMMON;  // Swap chain back buffers always start out in the common state.

	CreateResourceViews(allocation);
	NameResource(allocation->allocation, name);

	return std::move(allocation);
}

void ResourceManager::WriteBuffer(std::shared_ptr<Buffer>& target, const std::vector<uint8_t>& source, size_t targetOffset)
{
	if (target->description.updateRate == ResourceFrequency::Static)
	{
		VGScopedCPUStat("Buffer Write Static");

		VGAssert(target->description.accessFlags & AccessFlag::CPUWrite, "Failed to write to static buffer, no CPU write access.");
		VGAssert((target->description.size * target->description.stride) - targetOffset >= source.size(), "Failed to write to static buffer, source buffer is larger than target.");

		const auto frameIndex = device->GetFrameIndex();

		VGAssert(uploadOffsets[frameIndex] + source.size() <= uploadResources[frameIndex]->GetResource()->GetDesc().Width, "Failed to write to static buffer, exhausted frame upload heap.");

		std::memcpy(static_cast<uint8_t*>(uploadPtrs[frameIndex]) + uploadOffsets[frameIndex], source.data(), source.size());

		// Ensure we're in the proper state.
		if (target->state != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			device->GetDirectList().TransitionBarrier(target, D3D12_RESOURCE_STATE_COPY_DEST);
			device->GetDirectList().FlushBarriers();
		}
		
		auto* targetCommandList = device->GetDirectList().Native();  // Small writes are more efficiently performed on the direct/compute queue.
		targetCommandList->CopyBufferRegion(target->Native(), targetOffset, uploadResources[frameIndex]->GetResource(), uploadOffsets[frameIndex], source.size());

		uploadOffsets[frameIndex] += source.size();
	}

	else
	{
		VGScopedCPUStat("Buffer Write Dynamic");

		VGAssert(target->state == D3D12_RESOURCE_STATE_GENERIC_READ, "Dynamic buffers must always be in the generic read state.");
		VGAssert(target->description.accessFlags & AccessFlag::CPUWrite, "Failed to write to dynamic buffer, no CPU write access.");
		VGAssert((target->description.size * target->description.stride) - targetOffset >= source.size(), "Failed to write to dynamic buffer, source is larger than target.");

		void* mappedPtr = nullptr;

		D3D12_RANGE mapRange{ targetOffset, targetOffset + source.size() };

		auto result = target->Native()->Map(0, &mapRange, &mappedPtr);
		if (FAILED(result))
		{
			VGLogError(Rendering) << "Failed to map buffer resource during resource write: " << result;

			return;
		}

		std::memcpy(mappedPtr, source.data(), source.size());

		target->Native()->Unmap(0, nullptr);
	}
}

void ResourceManager::WriteTexture(std::shared_ptr<Texture>& target, const std::vector<uint8_t>& source)
{
	VGScopedCPUStat("Texture Write");

	VGAssert(target->description.accessFlags & AccessFlag::CPUWrite, "Failed to write to texture, no CPU write access.");
	VGAssert(target->description.width * target->description.height * target->description.depth <= source.size(), "Failed to write to texture, source is larger than target.");

	const auto frameIndex = device->GetFrameIndex();

	VGAssert(uploadOffsets[frameIndex] + source.size() <= uploadResources[frameIndex]->GetResource()->GetDesc().Width, "Failed to write to texture, exhausted frame upload heap.");

	std::memcpy(static_cast<uint8_t*>(uploadPtrs[frameIndex]) + uploadOffsets[frameIndex], source.data(), source.size());

	D3D12_TEXTURE_COPY_LOCATION sourceCopyDesc{};
	sourceCopyDesc.pResource = uploadResources[frameIndex]->GetResource();
	sourceCopyDesc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

	D3D12_RESOURCE_DESC targetDescriptionCopy = target->allocation->GetResource()->GetDesc();

	uint64_t requiredCopySize;
	device->Native()->GetCopyableFootprints(&targetDescriptionCopy, 0, 1, uploadOffsets[frameIndex], &sourceCopyDesc.PlacedFootprint, nullptr, nullptr, &requiredCopySize);

	D3D12_TEXTURE_COPY_LOCATION targetCopyDesc{};
	targetCopyDesc.pResource = target->Native();
	targetCopyDesc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	targetCopyDesc.SubresourceIndex = 0;

	// #TODO: Support custom copy sizes.
	D3D12_BOX sourceBox{};
	sourceBox.left = 0;
	sourceBox.top = 0;
	sourceBox.front = 0;
	sourceBox.right = target->description.width;
	sourceBox.bottom = target->description.height;
	sourceBox.back = target->description.depth;

	// Ensure we're in the proper state.
	if (target->state != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		device->GetDirectList().TransitionBarrier(target, D3D12_RESOURCE_STATE_COPY_DEST);
		device->GetDirectList().FlushBarriers();
	}

	auto* targetCommandList = device->GetDirectList().Native();  // Small writes are more efficiently performed on the direct/compute queue.
	targetCommandList->CopyTextureRegion(&targetCopyDesc, 0, 0, 0, &sourceCopyDesc, &sourceBox);

	uploadOffsets[frameIndex] += requiredCopySize;
}

void ResourceManager::CleanupFrameResources(size_t frame)
{
	VGScopedCPUStat("Cleanup Frame Resources");

	uploadOffsets[frame % frameCount] = 0;
}