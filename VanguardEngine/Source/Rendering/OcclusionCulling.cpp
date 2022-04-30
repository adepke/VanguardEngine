// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/OcclusionCulling.h>
#include <Rendering/Device.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Utility/Math.h>

#include <cmath>

void OcclusionCulling::Initialize(RenderDevice* inDevice)
{
	CvarCreate("hiZPyramidLevels", "How many mipmaps to generate for the depth pyramid, used in occlusion culling", 4);

	device = inDevice;
	lastFrameHiZ.id = 0;

	hiZLayout = RenderPipelineLayout{}
		.ComputeShader({ "GenerateHiZ", "Main" });  // Similar to generate mips, but enough differences to warrant a new shader.

#if ENABLE_EDITOR
	debugOverlayLayout = RenderPipelineLayout{}
		.VertexShader({ "HiZDebugOverlay", "VSMain" })
		.PixelShader({ "HiZDebugOverlay", "PSMain" });
#endif
}

void OcclusionCulling::Render(RenderGraph& graph, bool cameraFrozen, const RenderResource depthStencilTag)
{
	const auto [backBufferWidth, backBufferHeight] = graph.GetBackBufferResolution(device);
	const auto hiZMipLevels = std::min((int)std::floor(std::log2(std::max(backBufferWidth, backBufferHeight))), *CvarGet("hiZPyramidLevels", int)) + 1;

	TextureView hiZView{};
	std::vector<std::string> hiZViewNames;
	hiZViewNames.resize(hiZMipLevels);
	for (int i = 0; i < hiZMipLevels; ++i)
	{
		hiZViewNames[i] = std::string{ "uav_" } + std::to_string(i);
		hiZView.UAV(hiZViewNames[i], i);
	}

	auto& hiZPass = graph.AddPass("Hierarchical Z Pass", ExecutionQueue::Compute, !cameraFrozen);  // Disable Hi-Z updates when frozen.
	auto hiZTag = hiZPass.Create(TransientTextureDescription{
		.width = NextPowerOf2(backBufferWidth),
		.height = NextPowerOf2(backBufferHeight),
		.format = DXGI_FORMAT_R32_FLOAT,
		.mipMapping = true
	}, VGText("Hi-Z Depth pyramid"));
	hiZPass.Read(depthStencilTag, ResourceBind::SRV);
	hiZPass.Write(hiZTag, hiZView);
	hiZPass.Bind([&, hiZTag, depthStencilTag, hiZMipLevels, hiZViewNames](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipeline(hiZLayout);

		struct {
			uint32_t mipBase;
			uint32_t mipCount;
			XMFLOAT2 texelSize;
			uint32_t outputTextureIndices[4];
			uint32_t inputTextureIndex;
		} bindData;

		auto& depthComponent = device->GetResourceManager().Get(resources.GetTexture(depthStencilTag));
		const auto mipDispatches = static_cast<uint32_t>(std::ceil(hiZMipLevels / 4.f));
		for (int i = 0; i < mipDispatches; ++i)
		{
			const auto baseMipWidth = NextPowerOf2(depthComponent.description.width) >> i * 4;
			const auto baseMipHeight = NextPowerOf2(depthComponent.description.height) >> i * 4;

			bindData.mipBase = i * 4;  // Starting mip.
			bindData.mipCount = std::min(hiZMipLevels - bindData.mipBase, 4u);  // How many mips to generate (0, 4].
			bindData.texelSize = { 1.f / baseMipWidth, 1.f / baseMipHeight };
			bindData.inputTextureIndex = resources.Get(depthStencilTag);

			for (int j = 0; j < bindData.mipCount; ++j)
			{
				bindData.outputTextureIndices[j] = resources.Get(hiZTag, hiZViewNames[i * 4 + j]);
			}

			list.BindConstants("bindData", bindData);

			const auto dispatchX = std::max((uint32_t)std::ceil(baseMipWidth / (1.f * 8.f)), 1u);
			const auto dispatchY = std::max((uint32_t)std::ceil(baseMipHeight / (1.f * 8.f)), 1u);
			const auto dispatchZ = 1;

			list.Dispatch(dispatchX, dispatchY, dispatchZ);
			list.UAVBarrier(resources.GetTexture(hiZTag));
		}
	});

	++swapCount;
	lastFrameHiZ = hiZTag;
}

RenderResource OcclusionCulling::RenderDebugOverlay(RenderGraph& graph, const RenderResource cameraBufferTag)
{
#if ENABLE_EDITOR
	TextureView hiZView{};
	hiZView.SRV("", *CvarGet("hiZPyramidLevels", int) - 1, 1);

	auto& overlayPass = graph.AddPass("Occlusion Culling Debug Overlay", ExecutionQueue::Graphics);
	const auto debugOverlayTag = overlayPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Occlusion culling debug overlay"));
	overlayPass.Read(lastFrameHiZ, hiZView);
	overlayPass.Read(cameraBufferTag, ResourceBind::SRV);
	overlayPass.Output(debugOverlayTag, OutputBind::RTV, LoadType::Preserve);
	overlayPass.Bind([&, debugOverlayTag, cameraBufferTag](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipeline(debugOverlayLayout);

		struct
		{
			uint32_t hiZTexture;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
		} bindData;

		bindData.hiZTexture = resources.Get(lastFrameHiZ);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.

		list.BindConstants("bindData", bindData);

		list.DrawFullscreenQuad();
	});

	return debugOverlayTag;
#else
	return {};
#endif
}