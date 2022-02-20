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

void Bloom::Render(RenderGraph& graph, const RenderResource hdrSource)
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

	constexpr auto bloomDownsamples = 6;
	const auto [width, height] = graph.GetBackBufferResolution(device);
	const auto mipLevels = (int)std::floor(std::log2(std::max(width, height)));
	bloomPasses = std::min(bloomDownsamples, mipLevels - 1);

	TextureView downsampleExtractView{};
	std::vector<std::pair<std::string, std::string>> downsampleExtractViewNames;
	downsampleExtractViewNames.resize(bloomPasses);
	for (int i = 0; i < bloomPasses; ++i)
	{
		downsampleExtractViewNames[i] = std::make_pair(std::string{ "srv_" } + std::to_string(i), std::string{ "uav_" } + std::to_string(i));
		downsampleExtractView.SRV(downsampleExtractViewNames[i].first, i, 1);  // The input mip acts as the base level.
		downsampleExtractView.UAV(downsampleExtractViewNames[i].second, i + 1);
	}

	auto& downsamplePass = graph.AddPass("Bloom Downsample Pass", ExecutionQueue::Compute);
	downsamplePass.Write(extractTexture, downsampleExtractView);
	downsamplePass.Bind([this, extractTexture, downsampleExtractViewNames](CommandList& list, RenderPassResources& resources)
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

		for (int i = 0; i < bloomPasses; ++i)
		{
			std::string zoneName = "Downsample pass " + std::to_string(i + 1);
			VGScopedGPUTransientStat(zoneName.c_str(), device->GetDirectContext(), list.Native());

			bindData.inputTexture = resources.Get(extractTexture, downsampleExtractViewNames[i].first);
			bindData.outputTexture = resources.Get(extractTexture, downsampleExtractViewNames[i].second);
			list.BindConstants("bindData", bindData);

			const auto sizeFactor = std::pow(2.f, i + 1);  // Add one to dispatch in the dimensions of the output.
			uint32_t dispatchX = std::ceil(extractTextureComponent.description.width / (8.f * sizeFactor));
			uint32_t dispatchY = std::ceil(extractTextureComponent.description.height / (8.f * sizeFactor));

			list.Dispatch(dispatchX, dispatchY, 1);

			list.UAVBarrier(resources.GetTexture(extractTexture));
			list.FlushBarriers();
		}
	});

	TextureView upsampleExtractView{};
	upsampleExtractView.SRV("srv");
	std::vector<std::string> upsampleExtractViewNames;
	upsampleExtractViewNames.resize(bloomPasses);
	for (int i = 0; i < bloomPasses; ++i)
	{
		upsampleExtractViewNames[i] = std::string{ "srv_" } + std::to_string(i);
		upsampleExtractView.UAV(upsampleExtractViewNames[i], bloomPasses - i - 1);  // Write to the prior mip.
	}

	auto& compositionPass = graph.AddPass("Bloom Upsample Pass", ExecutionQueue::Compute);
	compositionPass.Read(extractTexture, upsampleExtractView);
	compositionPass.Write(hdrSource, TextureView{}
		.UAV("", 0));
	compositionPass.Bind([this, extractTexture, hdrSource, upsampleExtractViewNames](CommandList& list, RenderPassResources& resources)
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

		bindData.inputTexture = resources.Get(extractTexture, "srv");
		bindData.intensity = internalBlend;

		for (int i = 0; i < bloomPasses; ++i)
		{
			std::string zoneName = "Upsample pass " + std::to_string(i + 1);
			VGScopedGPUTransientStat(zoneName.c_str(), device->GetDirectContext(), list.Native());

			bindData.inputMip = bloomPasses - i;
			bindData.outputTexture = resources.Get(extractTexture, upsampleExtractViewNames[i]);
			list.BindConstants("bindData", bindData);

			const auto sizeFactor = std::pow(2.f, bloomPasses - i - 1);  // Dispatch in the dimensions of the output.
			uint32_t dispatchX = std::ceil(extractTextureComponent.description.width / (8.f * sizeFactor));
			uint32_t dispatchY = std::ceil(extractTextureComponent.description.height / (8.f * sizeFactor));

			list.Dispatch(dispatchX, dispatchY, 1);

			list.UAVBarrier(resources.GetTexture(extractTexture));
			list.FlushBarriers();
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