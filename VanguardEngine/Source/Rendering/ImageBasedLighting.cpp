// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/ImageBasedLighting.h>
#include <Rendering/Device.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/Resource.h>
#include <Rendering/ShaderStructs.h>

ImageBasedLighting::~ImageBasedLighting()
{
	device->GetResourceManager().Destroy(irradianceTexture);
	device->GetResourceManager().Destroy(prefilterTexture);
	device->GetResourceManager().Destroy(brdfTexture);
}

void ImageBasedLighting::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	ComputePipelineStateDescription convolutionState;
	convolutionState.shader = { "IBL/Convolution", "IrradianceMain" };
	convolutionState.macros.emplace_back("PREFILTER_LEVELS", prefilterLevels);
	irradiancePrecompute.Build(*device, convolutionState);

	convolutionState.shader = { "IBL/Convolution", "PrefilterMain" };
	prefilterPrecompute.Build(*device, convolutionState);

	convolutionState.shader = { "IBL/Convolution", "BRDFMain" };
	brdfPrecompute.Build(*device, convolutionState);

	TextureDescription irradianceDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = irradianceTextureSize,
		.height = irradianceTextureSize,
		.depth = 6,  // Texture cube.
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.mipMapping = false,
		.array = true
	};

	irradianceTexture = device->GetResourceManager().Create(irradianceDesc, VGText("IBL irradiance"));

	TextureDescription prefilterDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = prefilterTextureSize,
		.height = prefilterTextureSize,
		.depth = 6,  // Texture cube.
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.mipMapping = true,  // Roughness bins are stored in mip levels.
		.array = true
	};

	prefilterTexture = device->GetResourceManager().Create(prefilterDesc, VGText("IBL prefilter"));

	TextureDescription brdfDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = brdfTextureSize,
		.height = brdfTextureSize,
		.depth = 1,
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.mipMapping = false
	};

	brdfTexture = device->GetResourceManager().Create(brdfDesc, VGText("IBL BRDF"));
}

void ImageBasedLighting::UpdateLuts(RenderGraph& graph, RenderResource luminanceTexture, RenderResource cameraBuffer)
{
	if (!brdfRendered)
	{
		auto& brdfPass = graph.AddPass("IBL BRDF Pass", ExecutionQueue::Compute);
		brdfPass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
		{
			const auto& brdfComponent = device->GetResourceManager().Get(brdfTexture);

			D3D12_UNORDERED_ACCESS_VIEW_DESC brdfDesc{};
			brdfDesc.Format = brdfComponent.description.format;
			brdfDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			brdfDesc.Texture2D.MipSlice = 0;
			brdfDesc.Texture2D.PlaneSlice = 0;

			auto brdfUAV = device->AllocateDescriptor(DescriptorType::Default);
			device->Native()->CreateUnorderedAccessView(brdfComponent.allocation->GetResource(), nullptr, &brdfDesc, brdfUAV);

			struct BindData
			{
				uint32_t luminanceTexture;
				uint32_t convolutionTexture;
				uint32_t brdfTexture;
				uint32_t cubeFace;
				uint128_t prefilterMips[prefilterLevels];
			} bindData;

			bindData.brdfTexture = brdfUAV.bindlessIndex;
			std::vector<uint8_t> bindBytes;
			bindBytes.resize(sizeof(bindData));
			std::memcpy(bindBytes.data(), &bindData, bindBytes.size());

			const auto [handle, offset] = device->FrameAllocate(bindBytes.size());
			device->GetResourceManager().Write(handle, bindBytes, offset);

			list.BindPipelineState(brdfPrecompute);
			list.BindResource("bindData", handle, offset);
			list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());

			list.Dispatch(brdfTextureSize / 8, brdfTextureSize / 8, 1);

			device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(brdfUAV));
		});

		brdfRendered = true;
	}

	auto& irradiancePass = graph.AddPass("IBL Irradiance Pass", ExecutionQueue::Compute);
	irradiancePass.Read(luminanceTexture, ResourceBind::SRV);
	irradiancePass.Bind([&, luminanceTexture](CommandList& list, RenderGraphResourceManager& resources)
	{
		const auto& luminanceComponent = device->GetResourceManager().Get(resources.GetTexture(luminanceTexture));
		const auto& irradianceComponent = device->GetResourceManager().Get(irradianceTexture);

		D3D12_SHADER_RESOURCE_VIEW_DESC luminanceDesc{};
		luminanceDesc.Format = luminanceComponent.description.format;
		luminanceDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		luminanceDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		luminanceDesc.TextureCube.MostDetailedMip = 0;
		luminanceDesc.TextureCube.MipLevels = -1;
		luminanceDesc.TextureCube.ResourceMinLODClamp = 0.f;

		auto luminanceSRV = device->AllocateDescriptor(DescriptorType::Default);
		device->Native()->CreateShaderResourceView(luminanceComponent.allocation->GetResource(), &luminanceDesc, luminanceSRV);

		D3D12_UNORDERED_ACCESS_VIEW_DESC irradianceDesc{};
		irradianceDesc.Format = irradianceComponent.description.format;
		irradianceDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		irradianceDesc.Texture2DArray.MipSlice = 0;
		irradianceDesc.Texture2DArray.FirstArraySlice = 0;
		irradianceDesc.Texture2DArray.ArraySize = irradianceComponent.description.depth;
		irradianceDesc.Texture2DArray.PlaneSlice = 0;

		auto irradianceUAV = device->AllocateDescriptor(DescriptorType::Default);
		device->Native()->CreateUnorderedAccessView(irradianceComponent.allocation->GetResource(), nullptr, &irradianceDesc, irradianceUAV);

		struct BindData
		{
			uint32_t luminanceTexture;
			uint32_t irradianceTexture;
			uint32_t brdfTexture;
			uint32_t cubeFace;
			uint128_t prefilterMips[prefilterLevels];
		} bindData;
		
		bindData.luminanceTexture = luminanceSRV.bindlessIndex;
		bindData.irradianceTexture = irradianceUAV.bindlessIndex;
		std::vector<uint8_t> bindBytes;
		bindBytes.resize(sizeof(bindData));
		std::memcpy(bindBytes.data(), &bindData, bindBytes.size());

		const auto [handle, offset] = device->FrameAllocate(bindBytes.size());
		device->GetResourceManager().Write(handle, bindBytes, offset);

		list.BindPipelineState(irradiancePrecompute);
		list.BindResource("bindData", handle, offset);
		list.BindResourceTable("textureCubes", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("textureArraysRW", device->GetDescriptorAllocator().GetBindlessHeap());

		list.Dispatch(irradianceTextureSize / 8, irradianceTextureSize / 8, 6);

		device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(luminanceSRV));
		device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(irradianceUAV));
	});

	auto& prefilterPass = graph.AddPass("IBL Prefilter Pass", ExecutionQueue::Compute);
	prefilterPass.Read(luminanceTexture, ResourceBind::SRV);
	prefilterPass.Bind([&, luminanceTexture](CommandList& list, RenderGraphResourceManager& resources)
	{
		const auto& luminanceComponent = device->GetResourceManager().Get(resources.GetTexture(luminanceTexture));
		const auto& prefilterComponent = device->GetResourceManager().Get(prefilterTexture);

		D3D12_SHADER_RESOURCE_VIEW_DESC luminanceDesc{};
		luminanceDesc.Format = luminanceComponent.description.format;
		luminanceDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		luminanceDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		luminanceDesc.TextureCube.MostDetailedMip = 0;
		luminanceDesc.TextureCube.MipLevels = -1;
		luminanceDesc.TextureCube.ResourceMinLODClamp = 0.f;

		auto luminanceSRV = device->AllocateDescriptor(DescriptorType::Default);
		device->Native()->CreateShaderResourceView(luminanceComponent.allocation->GetResource(), &luminanceDesc, luminanceSRV);

		std::vector<DescriptorHandle> prefilterDescriptors;
		prefilterDescriptors.reserve(prefilterLevels);

		D3D12_UNORDERED_ACCESS_VIEW_DESC prefilterDesc{};
		prefilterDesc.Format = prefilterComponent.description.format;
		prefilterDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		prefilterDesc.Texture2DArray.FirstArraySlice = 0;
		prefilterDesc.Texture2DArray.ArraySize = prefilterComponent.description.depth;
		prefilterDesc.Texture2DArray.PlaneSlice = 0;

		for (int i = 0; i < prefilterLevels; ++i)
		{
			prefilterDesc.Texture2DArray.MipSlice = i;
			auto uav = device->AllocateDescriptor(DescriptorType::Default);
			device->Native()->CreateUnorderedAccessView(prefilterComponent.allocation->GetResource(), nullptr, &prefilterDesc, uav);
			prefilterDescriptors.emplace_back(std::move(uav));
		}

		struct BindData
		{
			uint32_t luminanceTexture;
			uint32_t irradianceTexture;
			uint32_t brdfTexture;
			uint32_t cubeFace;
			uint128_t prefilterMips[prefilterLevels];
		} bindData;
		
		bindData.luminanceTexture = luminanceSRV.bindlessIndex;
		bindData.cubeFace = slice;
		slice = (slice + 1) % 6;
		for (int i = 0; i < prefilterLevels; ++i)
		{
			bindData.prefilterMips[i] = prefilterDescriptors[i].bindlessIndex;
		}

		std::vector<uint8_t> bindBytes;
		bindBytes.resize(sizeof(bindData));
		std::memcpy(bindBytes.data(), &bindData, bindBytes.size());

		const auto [handle, offset] = device->FrameAllocate(bindBytes.size());
		device->GetResourceManager().Write(handle, bindBytes, offset);

		list.BindPipelineState(prefilterPrecompute);
		list.BindResource("bindData", handle, offset);
		list.BindResourceTable("textureCubes", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("textureArraysRW", device->GetDescriptorAllocator().GetBindlessHeap());

		list.Dispatch(prefilterTextureSize / 8, prefilterTextureSize / 8, 6);

		device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(luminanceSRV));

		for (auto&& descriptor : prefilterDescriptors)
		{
			device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(descriptor));
		}
	});
}