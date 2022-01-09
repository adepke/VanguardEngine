// Copyright (c) 2019-2021 Andrew Depke

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

	ComputePipelineStateDescription compositionStateDesc;
	compositionStateDesc.shader = { "Bloom/Composition.hlsl", "Main" };
	compositionState.Build(*device, compositionStateDesc);
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
	extractPass.Write(extractTexture, ResourceBind::UAV);
	extractPass.Bind([&, hdrSource, extractTexture](CommandList& list, RenderGraphResourceManager& resources)
	{
		list.BindPipelineState(extractState);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());

		const auto& sourceTextureComponent = device->GetResourceManager().Get(resources.GetTexture(hdrSource));
		const auto& extractTextureComponent = device->GetResourceManager().Get(resources.GetTexture(extractTexture));

		D3D12_UNORDERED_ACCESS_VIEW_DESC extractUavDesc{};
		extractUavDesc.Format = extractTextureComponent.description.format;
		extractUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		extractUavDesc.Texture2D.MipSlice = 0;
		extractUavDesc.Texture2D.PlaneSlice = 0;

		auto extractUav = device->AllocateDescriptor(DescriptorType::Default);
		device->Native()->CreateUnorderedAccessView(extractTextureComponent.allocation->GetResource(), nullptr, &extractUavDesc, extractUav);

		struct BindData
		{
			uint32_t inputTexture;
			uint32_t outputTexture;
			float threshold;
		} bindData;

		bindData.inputTexture = sourceTextureComponent.SRV->bindlessIndex;
		bindData.outputTexture = extractUav.bindlessIndex;
		bindData.threshold = 1.f;  // #TEMP
		list.BindConstants("bindData", bindData);

		uint32_t dispatchX = std::ceil(extractTextureComponent.description.width / 8.f);
		uint32_t dispatchY = std::ceil(extractTextureComponent.description.height / 8.f);

		list.Dispatch(dispatchX, dispatchY, 1);

		device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(extractUav));
	});

	auto& downsamplePass = graph.AddPass("Bloom Downsample Pass", ExecutionQueue::Compute);
	downsamplePass.Write(extractTexture, ResourceBind::UAV);
	downsamplePass.Bind([this, extractTexture](CommandList& list, RenderGraphResourceManager& resources)
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

	auto& compositionPass = graph.AddPass("Bloom Composition Pass", ExecutionQueue::Compute);
	compositionPass.Read(extractTexture, ResourceBind::SRV);
	compositionPass.Write(hdrSource, ResourceBind::UAV);
	compositionPass.Bind([this, extractTexture, hdrSource](CommandList& list, RenderGraphResourceManager& resources)
	{
		list.BindPipelineState(compositionState);
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());

		auto& extractTextureComponent = device->GetResourceManager().Get(resources.GetTexture(extractTexture));
		auto& hdrTextureComponent = device->GetResourceManager().Get(resources.GetTexture(hdrSource));

		struct BindData
		{
			uint32_t hdrSource;
			uint32_t bloomSamples;
			uint32_t bloomMips;
		} bindData;

		bindData.bloomSamples = extractTextureComponent.SRV->bindlessIndex;
		bindData.bloomMips = bloomPasses;
		
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = hdrTextureComponent.description.format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;

		auto uav = device->AllocateDescriptor(DescriptorType::Default);
		device->Native()->CreateUnorderedAccessView(hdrTextureComponent.allocation->GetResource(), nullptr, &uavDesc, uav);

		bindData.hdrSource = uav.bindlessIndex;

		list.BindConstants("bindData", bindData);

		uint32_t dispatchX = std::ceil(hdrTextureComponent.description.width / 8.f);
		uint32_t dispatchY = std::ceil(hdrTextureComponent.description.height / 8.f);

		list.Dispatch(dispatchX, dispatchY, 1);

		device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(uav));
	});
}