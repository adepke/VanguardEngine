// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/Bloom.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>

#include <vector>
#include <string>
#include <cmath>

void Bloom::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	ComputePipelineStateDescription extractStateDesc;
	extractStateDesc.shader = { "Bloom/Extract.hlsl", "Main" };
	extractState.Build(*device, extractStateDesc);

	ComputePipelineStateDescription downsampleStateDesc;
	downsampleStateDesc.shader = { "Bloom/Downsample.hlsl", "Main" };
	downsampleState.Build(*device, downsampleStateDesc);

	ComputePipelineStateDescription upsampleStateDesc;
	upsampleStateDesc.shader = { "Bloom/Upsample.hlsl", "Main" };
	upsampleState.Build(*device, upsampleStateDesc);
}

void Bloom::Render(RenderGraph& graph, RenderResource hdrSource)
{
	auto& extractPass = graph.AddPass("Bloom Extract Pass", ExecutionQueue::Compute);
	extractPass.Read(hdrSource, ResourceBind::SRV);
	const auto extractTexture = extractPass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.resolutionScale = 0.5f,
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT,  // #TODO: Remove alpha component.
		.mipMapping = true
	}, VGText("Bloom extraction output"));
	extractPass.Write(extractTexture, TextureView{}
		.UAV("", 0));
	extractPass.Bind([&, hdrSource, extractTexture](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipelineState(extractState);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());

		struct BindData
		{
			uint32_t inputTexture;
			uint32_t outputTexture;
		} bindData;

		bindData.inputTexture = resources.Get(hdrSource);
		bindData.outputTexture = resources.Get(extractTexture);
		list.BindConstants("bindData", bindData);

		const auto& extractTextureComponent = device->GetResourceManager().Get(resources.GetTexture(extractTexture));
		uint32_t dispatchX = std::ceil(extractTextureComponent.description.width / 8.f);
		uint32_t dispatchY = std::ceil(extractTextureComponent.description.height / 8.f);

		list.Dispatch(dispatchX, dispatchY, 1);
	});

	auto& downsamplePass = graph.AddPass("Bloom Downsample Pass", ExecutionQueue::Compute);
	downsamplePass.Write(extractTexture, ResourceBind::UAV);
	downsamplePass.Bind([this, extractTexture](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipelineState(downsampleState);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());

		auto& extractTextureComponent = device->GetResourceManager().Get(resources.GetTexture(extractTexture));

		struct BindData
		{
			uint32_t inputTexture;
			uint32_t outputTexture;
		} bindData;

		constexpr auto bloomDownsamples = 6;
		bloomPasses = std::min(bloomDownsamples, (int32_t)extractTextureComponent.Native()->GetDesc().MipLevels - 1);

		std::vector<DescriptorHandle> descriptors;
		descriptors.reserve(bloomPasses * 2);

		for (int i = 0; i < bloomPasses; ++i)
		{
			std::string zoneName = "Downsample pass " + std::to_string(i + 1);
			VGScopedGPUTransientStat(zoneName.c_str(), device->GetDirectContext(), list.Native());

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = extractTextureComponent.description.format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MostDetailedMip = i;
			srvDesc.Texture2D.MipLevels = 1;  // The input mip acts as the base level.
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

			auto srv = device->AllocateDescriptor(DescriptorType::Default);
			device->Native()->CreateShaderResourceView(extractTextureComponent.allocation->GetResource(), &srvDesc, srv);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = extractTextureComponent.description.format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = i + 1;
			uavDesc.Texture2D.PlaneSlice = 0;

			auto uav = device->AllocateDescriptor(DescriptorType::Default);
			device->Native()->CreateUnorderedAccessView(extractTextureComponent.allocation->GetResource(), nullptr, &uavDesc, uav);

			bindData.inputTexture = srv.bindlessIndex;
			bindData.outputTexture = uav.bindlessIndex;

			descriptors.emplace_back(std::move(srv));
			descriptors.emplace_back(std::move(uav));

			list.BindConstants("bindData", bindData);

			const auto sizeFactor = std::pow(2.f, i + 1);  // Add one to dispatch in the dimensions of the output.
			uint32_t dispatchX = std::ceil(extractTextureComponent.description.width / (8.f * sizeFactor));
			uint32_t dispatchY = std::ceil(extractTextureComponent.description.height / (8.f * sizeFactor));

			list.Dispatch(dispatchX, dispatchY, 1);

			list.UAVBarrier(resources.GetTexture(extractTexture));
			list.FlushBarriers();
		}

		for (auto&& descriptor : descriptors)
		{
			device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(descriptor));
		}
	});

	auto& compositionPass = graph.AddPass("Bloom Upsample Pass", ExecutionQueue::Compute);
	compositionPass.Read(extractTexture, ResourceBind::SRV);
	compositionPass.Write(hdrSource, TextureView{}
		.UAV("", 0));
	compositionPass.Bind([this, extractTexture, hdrSource](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipelineState(upsampleState);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());

		auto& extractTextureComponent = device->GetResourceManager().Get(resources.GetTexture(extractTexture));
		auto& hdrTextureComponent = device->GetResourceManager().Get(resources.GetTexture(hdrSource));

		struct BindData
		{
			uint32_t inputTexture;
			uint32_t inputMip;
			uint32_t outputTexture;
			float intensity;
		} bindData;

		bindData.inputTexture = resources.Get(extractTexture);
		bindData.intensity = internalBlend;

		for (int i = 0; i < bloomPasses; ++i)
		{
			std::string zoneName = "Upsample pass " + std::to_string(i + 1);
			VGScopedGPUTransientStat(zoneName.c_str(), device->GetDirectContext(), list.Native());

			bindData.inputMip = bloomPasses - i;

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = extractTextureComponent.description.format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = bindData.inputMip - 1;  // Write to the prior mip.
			uavDesc.Texture2D.PlaneSlice = 0;

			auto uav = device->AllocateDescriptor(DescriptorType::Default);
			device->Native()->CreateUnorderedAccessView(extractTextureComponent.allocation->GetResource(), nullptr, &uavDesc, uav);

			bindData.outputTexture = uav.bindlessIndex;

			list.BindConstants("bindData", bindData);

			const auto sizeFactor = std::pow(2.f, bloomPasses - i - 1);  // Dispatch in the dimensions of the output.
			uint32_t dispatchX = std::ceil(extractTextureComponent.description.width / (8.f * sizeFactor));
			uint32_t dispatchY = std::ceil(extractTextureComponent.description.height / (8.f * sizeFactor));

			list.Dispatch(dispatchX, dispatchY, 1);

			list.UAVBarrier(resources.GetTexture(extractTexture));
			list.FlushBarriers();

			device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(uav));
		}

		VGScopedGPUStat("Upsample composition", device->GetDirectContext(), list.Native());

		bindData.inputMip = 0;
		bindData.outputTexture = resources.Get(hdrSource);
		bindData.intensity = intensity;

		list.BindConstants("bindData", bindData);

		uint32_t dispatchX = std::ceil(hdrTextureComponent.description.width / 8.f);
		uint32_t dispatchY = std::ceil(hdrTextureComponent.description.height / 8.f);

		list.Dispatch(dispatchX, dispatchY, 1);
	});
}