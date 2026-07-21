// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/Shadows.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderPipeline.h>
#include <Rendering/RenderUtils.h>
#include <Core/ConsoleVariable.h>

#include <algorithm>

RayTracedShadows::~RayTracedShadows()
{
	if (!device)
	{
		return;
	}

	auto& resourceManager = device->GetResourceManager();

	for (auto& texture : history)
	{
		if (resourceManager.Valid(texture))
		{
			resourceManager.Destroy(texture);
		}
	}
}

void RayTracedShadows::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	CvarCreate("rtShadowsEnabled", "Enables ray traced sun shadows (requires rayTracingEnabled). 0=off, 1=on", 1);
	CvarCreate("rtShadowDenoiserEnabled", "Enables denoising of the ray traced shadows. 0=off, 1=on", 1);
	CvarCreate("rtShadowSpatialRadius", "Filter radius of the shadow denoiser spatial pass, in pixels", 3);
	CvarCreate("rtDebugView", "Ray tracing debug visualization. 0=off, 1=instances, 2=hit distance, 3=barycentrics", 0);
}

void RayTracedShadows::EnsureHistory(uint32_t width, uint32_t height)
{
	if (width == historyWidth && height == historyHeight)
	{
		return;
	}

	auto& resourceManager = device->GetResourceManager();

	for (auto& texture : history)
	{
		if (resourceManager.Valid(texture))
		{
			resourceManager.AddFrameResource(device->GetFrameIndex(), texture);
		}
	}

	TextureDescription description{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = width,
		.height = height,
		.format = DXGI_FORMAT_R16G16_FLOAT
	};

	history[0] = resourceManager.Create(description, VGText("Sun shadow history 0"));
	history[1] = resourceManager.Create(description, VGText("Sun shadow history 1"));

	historyWidth = width;
	historyHeight = height;
	historyValid = false;
}

RenderResource RayTracedShadows::Render(RenderGraph& graph, RenderResource tlasTag, RenderResource depthTag,
	RenderResource normalTag, RenderResource cameraBufferTag, XMFLOAT3 sunDirection, uint32_t frame)
{
	VGScopedCPUStat("Ray Traced Shadows");

	const uint32_t width = device->renderWidth;
	const uint32_t height = device->renderHeight;
	const auto dispatchX = (width + 7) / 8;
	const auto dispatchY = (height + 7) / 8;

	const auto blueNoiseTag = graph.Import(RenderUtils::Get().blueNoise);

	// Trace one shadow ray per pixel towards a jittered point on the sun disk.
	auto& tracePass = graph.AddPass("Sun Shadow Trace Pass", ExecutionQueue::Compute);
	const auto shadowRawTag = tracePass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16_FLOAT
	}, VGText("Sun shadow raw"));
	tracePass.Read(depthTag, ResourceBind::SRV);
	tracePass.Read(normalTag, ResourceBind::SRV);
	tracePass.Read(cameraBufferTag, ResourceBind::SRV);
	tracePass.Read(blueNoiseTag, ResourceBind::SRV);
	tracePass.Read(tlasTag, ResourceBind::AS);
	tracePass.Write(shadowRawTag, TextureView{}.UAV("", 0));
	tracePass.Bind([this, depthTag, normalTag, cameraBufferTag, blueNoiseTag, tlasTag, shadowRawTag,
		sunDirection, frame, width, height, dispatchX, dispatchY](CommandList& list, RenderPassResources& resources)
	{
		auto layout = RenderPipelineLayout{}
			.ComputeShader({ "RayTracing/TraceSunShadows", "Main" });

		list.BindPipeline(layout);

		struct {
			uint32_t depthTexture;
			uint32_t normalTexture;
			uint32_t outputTexture;
			uint32_t accelerationStructure;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t outputResolution[2];
			uint32_t blueNoiseTexture;
			XMFLOAT3 sunDirection;
			uint32_t timeSlice;
		} bindData;

		bindData.depthTexture = resources.Get(depthTag);
		bindData.normalTexture = resources.Get(normalTag);
		bindData.outputTexture = resources.Get(shadowRawTag);
		bindData.accelerationStructure = resources.Get(tlasTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.outputResolution[0] = width;
		bindData.outputResolution[1] = height;
		bindData.blueNoiseTexture = resources.Get(blueNoiseTag);
		bindData.sunDirection = sunDirection;
		bindData.timeSlice = frame;

		list.BindConstants("bindData", bindData);

		list.Dispatch(dispatchX, dispatchY, 1);
	});

	if (*CvarGet("rtShadowDenoiserEnabled", int) == 0)
	{
		// Raw 1spp output. Invalidate the accumulation so re-enabling the denoiser starts fresh.
		historyValid = false;
		return shadowRawTag;
	}

	EnsureHistory(width, height);

	const bool useHistory = historyValid;
	const auto historyReadTag = graph.Import(history[historyIndex]);
	const auto historyWriteTag = graph.Import(history[1 - historyIndex]);

	// Temporal accumulation.
	auto& temporalPass = graph.AddPass("Sun Shadow Temporal Pass", ExecutionQueue::Compute);
	temporalPass.Read(shadowRawTag, ResourceBind::SRV);
	temporalPass.Read(depthTag, ResourceBind::SRV);
	temporalPass.Read(cameraBufferTag, ResourceBind::SRV);
	temporalPass.Read(historyReadTag, ResourceBind::SRV);
	temporalPass.Write(historyWriteTag, TextureView{}.UAV("", 0));
	temporalPass.Bind([this, shadowRawTag, depthTag, cameraBufferTag, historyReadTag, historyWriteTag,
		useHistory, width, height, dispatchX, dispatchY](CommandList& list, RenderPassResources& resources)
	{
		auto layout = RenderPipelineLayout{}
			.ComputeShader({ "RayTracing/ShadowDenoiseTemporal", "Main" });

		list.BindPipeline(layout);

		struct {
			uint32_t currentTexture;
			uint32_t historyTexture;
			uint32_t outputTexture;
			uint32_t depthTexture;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t historyValid;
			float blendFactor;
			uint32_t outputResolution[2];
		} bindData;

		bindData.currentTexture = resources.Get(shadowRawTag);
		bindData.historyTexture = resources.Get(historyReadTag);
		bindData.outputTexture = resources.Get(historyWriteTag);
		bindData.depthTexture = resources.Get(depthTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.historyValid = useHistory ? 1 : 0;
		bindData.blendFactor = 0.1f;
		bindData.outputResolution[0] = width;
		bindData.outputResolution[1] = height;

		list.BindConstants("bindData", bindData);

		list.Dispatch(dispatchX, dispatchY, 1);
	});

	// Edge-aware spatial filter.
	auto& spatialPass = graph.AddPass("Sun Shadow Spatial Pass", ExecutionQueue::Compute);
	const auto shadowMaskTag = spatialPass.Create(TransientTextureDescription{
		.format = DXGI_FORMAT_R16G16_FLOAT
	}, VGText("Sun shadow mask"));
	spatialPass.Read(historyWriteTag, ResourceBind::SRV);
	spatialPass.Read(normalTag, ResourceBind::SRV);
	spatialPass.Write(shadowMaskTag, TextureView{}.UAV("", 0));
	spatialPass.Bind([this, historyWriteTag, normalTag, shadowMaskTag, width, height, dispatchX, dispatchY]
		(CommandList& list, RenderPassResources& resources)
	{
		auto layout = RenderPipelineLayout{}
			.ComputeShader({ "RayTracing/ShadowDenoiseSpatial", "Main" });

		list.BindPipeline(layout);

		struct {
			uint32_t inputTexture;
			uint32_t normalTexture;
			uint32_t outputTexture;
			uint32_t filterRadius;
			uint32_t outputResolution[2];
		} bindData;

		bindData.inputTexture = resources.Get(historyWriteTag);
		bindData.normalTexture = resources.Get(normalTag);
		bindData.outputTexture = resources.Get(shadowMaskTag);
		bindData.filterRadius = std::max(*CvarGet("rtShadowSpatialRadius", int), 0);
		bindData.outputResolution[0] = width;
		bindData.outputResolution[1] = height;

		list.BindConstants("bindData", bindData);

		list.Dispatch(dispatchX, dispatchY, 1);
	});

	historyIndex = 1 - historyIndex;
	historyValid = true;

	return shadowMaskTag;
}

void RayTracedShadows::RenderDebug(RenderGraph& graph, RenderResource tlasTag, RenderResource cameraBufferTag,
	RenderResource outputTag, int debugMode)
{
	const uint32_t width = device->renderWidth;
	const uint32_t height = device->renderHeight;

	auto& debugPass = graph.AddPass("Ray Tracing Debug Pass", ExecutionQueue::Compute);
	debugPass.Read(tlasTag, ResourceBind::AS);
	debugPass.Read(cameraBufferTag, ResourceBind::SRV);
	debugPass.Write(outputTag, TextureView{}.UAV("", 0));
	debugPass.Bind([this, tlasTag, cameraBufferTag, outputTag, debugMode, width, height]
		(CommandList& list, RenderPassResources& resources)
	{
		auto layout = RenderPipelineLayout{}
			.ComputeShader({ "RayTracing/DebugTrace", "Main" });

		list.BindPipeline(layout);

		struct {
			uint32_t outputTexture;
			uint32_t accelerationStructure;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t outputResolution[2];
			uint32_t debugMode;
		} bindData;

		bindData.outputTexture = resources.Get(outputTag);
		bindData.accelerationStructure = resources.Get(tlasTag);
		bindData.cameraBuffer = resources.Get(cameraBufferTag);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.outputResolution[0] = width;
		bindData.outputResolution[1] = height;
		bindData.debugMode = static_cast<uint32_t>(debugMode);

		list.BindConstants("bindData", bindData);

		list.Dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
