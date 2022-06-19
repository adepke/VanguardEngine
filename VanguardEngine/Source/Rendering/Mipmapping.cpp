// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/Mipmapping.h>
#include <Rendering/Base.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/ResourceFormat.h>
#include <Utility/Math.h>

#include <vector>
#include <cmath>

void Mipmapper::Initialize(RenderDevice& device)
{
	layout2dState.Build(device, ComputePipelineStateDescription{
		.shader = { "Utils/Mipmap2d", "Main" }
	});

	layout3dState.Build(device, ComputePipelineStateDescription{
		.shader = { "Utils/Mipmap3d", "Main" }
	});
}

void Mipmapper::Generate2D(RenderDevice& device, CommandList& list, TextureHandle texture, TextureComponent& component)
{
	const auto layers = component.description.depth;
	const auto mipLevels = component.allocation->GetResource()->GetDesc().MipLevels;
	const auto mipDispatches = static_cast<uint32_t>(std::ceil(mipLevels / 4.f));

	std::vector<DescriptorHandle> uavDescriptors;
	uavDescriptors.reserve(layers * (mipLevels - 1));

	for (int i = 0; i < layers; ++i)
	{
		for (int j = 0; j < mipDispatches; ++j)
		{
			const auto baseMipWidth = NextPowerOf2(component.description.width) >> j * 4;
			const auto baseMipHeight = NextPowerOf2(component.description.height) >> j * 4;

			struct BindData
			{
				uint32_t mipBase;
				uint32_t mipCount;
				XMFLOAT2 texelSize;
				// Boundary
				uint32_t outputTextureIndices[4];
				// Boundary
				uint32_t inputTextureIndex;
				uint32_t sRGB;
				uint32_t resourceType;
				uint32_t layer;
			} bindData;

			bindData.mipBase = j * 4;  // Starting mip.
			bindData.mipCount = std::min(mipLevels - bindData.mipBase - 1, 4u);  // How many mips to generate (0, 4].
			bindData.sRGB = IsResourceFormatSRGB(component.description.format);
			bindData.texelSize = { 2.f / baseMipWidth, 2.f / baseMipHeight };
			bindData.inputTextureIndex = component.SRV->bindlessIndex;
			bindData.resourceType = layers > 1 ? (component.description.array ? 1 : 2) : 0;
			bindData.layer = i;

			// Allocate UAVs.
			for (int k = 0; k < bindData.mipCount; ++k)
			{
				auto descriptor = device.AllocateDescriptor(DescriptorType::Default);

				D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
				viewDesc.Format = ConvertResourceFormatToLinear(component.description.format);
				if (layers == 1)
				{
					viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					viewDesc.Texture2D.MipSlice = j * 4 + k + 1;
					viewDesc.Texture2D.PlaneSlice = 0;
				}

				else
				{
					if (component.description.array)
					{
						viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
						viewDesc.Texture2DArray.MipSlice = j * 4 + k + 1;
						viewDesc.Texture2DArray.FirstArraySlice = 0;
						viewDesc.Texture2DArray.ArraySize = component.description.depth;
						viewDesc.Texture2DArray.PlaneSlice = 0;
					}

					else
					{
						viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
						viewDesc.Texture3D.MipSlice = j * 4 + k + 1;
						viewDesc.Texture3D.FirstWSlice = 0;
						viewDesc.Texture3D.WSize = -1;
					}
				}

				device.Native()->CreateUnorderedAccessView(component.allocation->GetResource(), nullptr, &viewDesc, descriptor);

				bindData.outputTextureIndices[k] = descriptor.bindlessIndex;

				uavDescriptors.emplace_back(std::move(descriptor));
			}

			list.BindPipelineState(layout2dState);
			list.BindDescriptorAllocator(device.GetDescriptorAllocator());
			list.BindConstants("bindData", bindData);

			list.Dispatch(std::max((uint32_t)std::ceil(baseMipWidth / (2.f * 8.f)), 1u), std::max((uint32_t)std::ceil(baseMipHeight / (2.f * 8.f)), 1u), 1);

			list.UAVBarrier(texture);
			list.FlushBarriers();
		}
	}

	for (auto&& descriptor : uavDescriptors)
	{
		device.GetResourceManager().AddFrameDescriptor(device.GetFrameIndex(), std::move(descriptor));
	}
}

void Mipmapper::Generate3D(RenderDevice& device, CommandList& list, TextureHandle texture, TextureComponent& component)
{
	VGAssert(component.description.width == component.description.height && component.description.height == component.description.depth, "3D texture must be cubes for mipmapping.");
	VGAssert(IsPowerOf2(component.description.width), "3D textures must be power of 2 for mipmapping.");

	const auto mipLevels = component.Native()->GetDesc().MipLevels;
	for (int i = 0; i < mipLevels - 1; ++i)
	{
		const float baseMipWidth = component.description.width >> i;
		const float baseMipHeight = component.description.height >> i;
		const float baseMipDepth = component.description.depth >> i;

		struct {
			uint32_t mipBase;
			uint32_t mipCount;
			uint32_t inputTextureIndex;
			uint32_t sRGB;
			// Boundary
			uint32_t outputTextureIndex;
			XMFLOAT3 texelSize;
		} bindData;

		bindData.mipBase = i;
		bindData.mipCount = 1;  // #TODO: Compute multiple mip levels in a single dispatch.
		bindData.sRGB = IsResourceFormatSRGB(component.description.format);
		bindData.texelSize = { 2.f / baseMipWidth, 2.f / baseMipHeight, 2.f / baseMipDepth };
		bindData.inputTextureIndex = component.SRV->bindlessIndex;
		
		auto descriptor = device.AllocateDescriptor(DescriptorType::Default);

		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{
			.Format = component.description.format,
			.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D,
			.Texture3D = {
				.MipSlice = i + 1u,
				.FirstWSlice = 0,
				.WSize = -1u
			}
		};

		device.Native()->CreateUnorderedAccessView(component.allocation->GetResource(), nullptr, &viewDesc, descriptor);
		
		bindData.outputTextureIndex = descriptor.bindlessIndex;

		device.GetResourceManager().AddFrameDescriptor(device.GetFrameIndex(), std::move(descriptor));

		list.BindPipelineState(layout3dState);
		list.BindDescriptorAllocator(device.GetDescriptorAllocator());
		list.BindConstants("bindData", bindData);

		auto dispatchX = std::max((uint32_t)std::ceil(baseMipWidth / (2.f * 8.f)), 1u);
		auto dispatchY = std::max((uint32_t)std::ceil(baseMipHeight / (2.f * 8.f)), 1u);
		auto dispatchZ = std::max((uint32_t)std::ceil(baseMipDepth / 2.f), 1u);
		list.Dispatch(dispatchX, dispatchY, dispatchZ);

		list.UAVBarrier(texture);
		list.FlushBarriers();
	}
}