// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/Clouds.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/Atmosphere.h>
#include <Rendering/RenderUtils.h>
#include <Asset/TextureLoader.h>
#include <Core/Config.h>
#include <Utility/Math.h>

void Clouds::GenerateWeather(CommandList& list, uint32_t weatherTexture)
{
	list.BindPipeline(weatherLayout);

	struct {
		uint32_t weatherTexture;
		float globalCoverage;
		float precipitation;
		float time;
		XMFLOAT2 wind;
	} bindData;

	bindData.weatherTexture = weatherTexture;
	bindData.globalCoverage = coverage;
	bindData.precipitation = precipitation;
	bindData.wind = { windDirection.x * windStrength, windDirection.y * windStrength };
	bindData.time = Renderer::Get().GetAppTime();

	list.BindConstants("bindData", bindData);

	auto dispatchX = std::ceil((float)weatherSize / 8);
	auto dispatchY = std::ceil((float)weatherSize / 8);

	list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, 1);
}

void Clouds::GenerateNoise(CommandList& list, uint32_t baseShapeTexture, uint32_t detailShapeTexture, uint32_t curlShapeTexture)
{
	struct {
		uint32_t outputTexture;
	} bindData;

	list.BindPipeline(baseNoiseLayout);
	bindData.outputTexture = baseShapeTexture;
	list.BindConstants("bindData", bindData);
	list.Dispatch(1, 1, 1);

	list.BindPipeline(detailNoiseLayout);
	bindData.outputTexture = detailShapeTexture;
	list.BindConstants("bindData", bindData);
	list.Dispatch(1, 1, 1);

	list.BindPipeline(curlNoiseLayout);
	bindData.outputTexture = curlShapeTexture;
	list.BindConstants("bindData", bindData);
	list.Dispatch(1, 1, 1);
}

Clouds::~Clouds()
{
	device->GetResourceManager().Destroy(baseShapeNoise);
	device->GetResourceManager().Destroy(detailShapeNoise);
	device->GetResourceManager().Destroy(curlShapeNoise);
}

void Clouds::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	CvarCreate("cloudRayMarchQuality", "Controls the ray march quality of the clouds. Increasing quality degrades performance. 0=lowDetail, 1=default, 2=groundTruth", 1);
	CvarCreate("cloudRenderScale", "Controls the render scale of the volumetric clouds", 0.25f);
	CvarCreate("cloudDebugMarchCount", "Debug cloud ray march steps", 0);
	CvarCreate("cloudDebugTransmittance", "Debug cloud transmittance", 0);

	weatherLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Weather", "Main" });

	baseNoiseLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Shapes", "BaseShapeMain" });

	detailNoiseLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Shapes", "DetailShapeMain" });

	curlNoiseLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Shapes", "CurlNoiseMain" });

	TextureDescription weatherDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = weatherSize,
		.height = weatherSize,
		.depth = 1,
		.format = DXGI_FORMAT_R11G11B10_FLOAT
	};
	weather = device->GetResourceManager().Create(weatherDesc, VGText("Clouds weather"));

	TextureDescription baseShapeNoiseDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = 128,
		.height = 128,
		.depth = 128,
		.format = DXGI_FORMAT_R8_UNORM,
		.mipMapping = true
	};
	baseShapeNoise = device->GetResourceManager().Create(baseShapeNoiseDesc, VGText("Clouds base shape noise"));

	TextureDescription detailShapeNoiseDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = 32,
		.height = 32,
		.depth = 32,
		.format = DXGI_FORMAT_R8_UNORM
	};
	detailShapeNoise = device->GetResourceManager().Create(detailShapeNoiseDesc, VGText("Clouds detail shape noise"));

	TextureDescription curlShapeNoiseDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = 48,
		.height = 48,
		.depth = 48,
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	};
	curlShapeNoise = device->GetResourceManager().Create(curlShapeNoiseDesc, VGText("Clouds curl shape noise"));

	cirrusClouds = AssetLoader::LoadTexture(*device, Config::utilitiesPath / "Cirrus4k.png", false);

	lastFrameScatteringUpscaled.id = 0;
	lastFrameDepthUpscaled.id = 0;
	lastFrameVisibilityUpscaled.id = 0;
}

CloudResources Clouds::Render(RenderGraph& graph, entt::registry& registry, const Atmosphere& atmosphere, const RenderResource cameraBuffer, const RenderResource depthStencil, const RenderResource atmosphereIrradiance)
{
	const auto weatherTag = graph.Import(weather);
	const auto baseShapeNoiseTag = graph.Import(baseShapeNoise);
	const auto detailShapeNoiseTag = graph.Import(detailShapeNoise);
	const auto curlShapeNoiseTag = graph.Import(curlShapeNoise);
	const auto cirrusTag = graph.Import(cirrusClouds);
	const auto blueNoiseTag = graph.Import(RenderUtils::Get().blueNoise);

	auto solarZenithAngle = 0.f;
	if (registry.valid(atmosphere.sunLight))
	{
		solarZenithAngle = registry.get<TimeOfDayComponent>(atmosphere.sunLight).solarZenithAngle;
	}

	if (dirty)
	{
		auto& noisePass = graph.AddPass("Clouds Noise Pass", ExecutionQueue::Compute);
		noisePass.Write(baseShapeNoiseTag, TextureView{}.UAV("", 0));
		noisePass.Write(detailShapeNoiseTag, TextureView{}.UAV("", 0));
		noisePass.Write(curlShapeNoiseTag, TextureView{}.UAV("", 0));
		noisePass.Bind([this, baseShapeNoiseTag, detailShapeNoiseTag, curlShapeNoiseTag](CommandList& list, RenderPassResources& resources)
		{
			GenerateNoise(list, resources.Get(baseShapeNoiseTag), resources.Get(detailShapeNoiseTag), resources.Get(curlShapeNoiseTag));

			list.UAVBarrier(baseShapeNoise);
			list.FlushBarriers();

			// Mipmap the base shape noise for local density information.
			device->GetResourceManager().GenerateMipmaps(list, baseShapeNoise);
		});

		dirty = false;
	}

	auto& weatherPass = graph.AddPass("Weather Pass", ExecutionQueue::Compute);
	weatherPass.Write(weatherTag, TextureView{}.UAV("", 0));
	weatherPass.Bind([this, weatherTag](CommandList& list, RenderPassResources& resources)
	{
		GenerateWeather(list, resources.Get(weatherTag));
	});

	const float cloudRenderScale = *CvarGet("cloudRenderScale", float);

	auto& cloudsPass = graph.AddPass("Clouds Pass", ExecutionQueue::Graphics);
	const auto cloudOutput = cloudsPass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = cloudRenderScale,
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Clouds scattering transmittance"));
	const auto cloudDepth = cloudsPass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = cloudRenderScale,
		.format = DXGI_FORMAT_R32_FLOAT
	}, VGText("Clouds depth"));
	cloudsPass.Read(cameraBuffer, ResourceBind::SRV);
	cloudsPass.Read(weatherTag, ResourceBind::SRV);
	cloudsPass.Read(baseShapeNoiseTag, ResourceBind::SRV);
	cloudsPass.Read(detailShapeNoiseTag, ResourceBind::SRV);
	cloudsPass.Read(curlShapeNoiseTag, ResourceBind::SRV);
	cloudsPass.Read(depthStencil, ResourceBind::SRV);
	cloudsPass.Read(blueNoiseTag, ResourceBind::SRV);
	cloudsPass.Read(atmosphereIrradiance, ResourceBind::SRV);
	cloudsPass.Output(cloudOutput, OutputBind::RTV, LoadType::Preserve);
	cloudsPass.Write(cloudDepth, TextureView{}.UAV("", 0));
	cloudsPass.Bind([this, weatherTag, baseShapeNoiseTag, detailShapeNoiseTag, curlShapeNoiseTag, solarZenithAngle,
		cameraBuffer, depthStencil, cloudOutput, blueNoiseTag, cloudDepth, atmosphereIrradiance]
		(CommandList& list, RenderPassResources& resources)
	{
		auto cloudsLayout = RenderPipelineLayout{}
			.VertexShader({ "Clouds/Main", "VSMain" })
			.PixelShader({ "Clouds/Main", "PSMain" })
			.BlendMode(false, BlendMode{})
			.DepthEnabled(false);

		if (*CvarGet("cloudRayMarchQuality", int) < 1)
			cloudsLayout.Macro({ "CLOUDS_LOW_DETAIL" });
		else if (*CvarGet("cloudRayMarchQuality", int) > 1)
			cloudsLayout.Macro({ "CLOUDS_MARCH_GROUND_TRUTH_DETAIL" });
		if (*CvarGet("cloudDebugMarchCount", int) > 0)
			cloudsLayout.Macro({ "CLOUDS_DEBUG_MARCHCOUNT" });

		list.BindPipeline(cloudsLayout);

		struct {
			uint32_t weatherTexture;
			uint32_t baseShapeNoiseTexture;
			uint32_t detailShapeNoiseTexture;
			uint32_t curlShapeNoiseTexture;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			float solarZenithAngle;
			uint32_t timeSlice;
			uint32_t depthTexture;
			uint32_t geometryDepthTexture;
			uint32_t blueNoiseTexture;
			uint32_t atmosphereIrradianceBuffer;
			uint32_t outputResolution[2];
			uint32_t upscaledResolution[2];
			float time;
			XMFLOAT2 wind;
		} bindData;

		bindData.weatherTexture = resources.Get(weatherTag);
		bindData.baseShapeNoiseTexture = resources.Get(baseShapeNoiseTag);
		bindData.detailShapeNoiseTexture = resources.Get(detailShapeNoiseTag);
		bindData.curlShapeNoiseTexture = resources.Get(curlShapeNoiseTag);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.timeSlice = Renderer::Get().GetAppFrame() % 16;
		bindData.depthTexture = resources.Get(cloudDepth);
		bindData.geometryDepthTexture = resources.Get(depthStencil);
		bindData.blueNoiseTexture = resources.Get(blueNoiseTag);
		bindData.atmosphereIrradianceBuffer = resources.Get(atmosphereIrradiance);
		bindData.time = Renderer::Get().GetAppTime();
		bindData.wind = { windDirection.x * windStrength, windDirection.y * windStrength };

		const auto& cloudOutputComponent = device->GetResourceManager().Get(resources.GetTexture(cloudOutput));
		bindData.outputResolution[0] = cloudOutputComponent.description.width;
		bindData.outputResolution[1] = cloudOutputComponent.description.height;

		bindData.upscaledResolution[0] = device->renderWidth;
		bindData.upscaledResolution[1] = device->renderHeight;

		list.BindConstants("bindData", bindData);
		list.DrawFullscreenQuad();
	});

	auto visibilityEnabled = *CvarGet("renderLightShafts", int);

	auto& visibilityPass = graph.AddPass("Clouds Sky Visibility Pass", ExecutionQueue::Compute, visibilityEnabled > 0);
	const auto cloudVisibility = visibilityPass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = cloudRenderScale,
		.format = DXGI_FORMAT_R16G16_FLOAT
	}, VGText("Clouds visibility map"));
	visibilityPass.Read(cameraBuffer, ResourceBind::SRV);
	visibilityPass.Read(weatherTag, ResourceBind::SRV);
	visibilityPass.Read(baseShapeNoiseTag, ResourceBind::SRV);
	visibilityPass.Read(curlShapeNoiseTag, ResourceBind::SRV);
	visibilityPass.Read(depthStencil, ResourceBind::SRV);
	visibilityPass.Read(blueNoiseTag, ResourceBind::SRV);
	visibilityPass.Read(atmosphereIrradiance, ResourceBind::SRV);
	visibilityPass.Write(cloudVisibility, TextureView{}
		.UAV("", 0));
	visibilityPass.Bind([this, cameraBuffer, weatherTag, baseShapeNoiseTag, curlShapeNoiseTag, depthStencil, blueNoiseTag, atmosphereIrradiance,
		cloudVisibility, solarZenithAngle](CommandList& list, RenderPassResources& resources)
	{
		auto visibilityLayout = RenderPipelineLayout{}
			.ComputeShader({ "Clouds/Visibility", "Main" })
			.Macro({ "CLOUDS_LOW_DETAIL" });  // Always low detail, no matter the quality setting
			// Interestingly, applying the ONLY_DEPTH macro does not appear to help performance. The issue there is likely the transmittance
			// approximation being too conservative and allowing too many steps into the cloud. However, if this is done then small clouds
			// will yield too much shadow and does not look visibily correct.
			//.Macro({ "CLOUDS_ONLY_DEPTH" });
		
		if (*CvarGet("cloudDebugMarchCount", int) > 0)
			visibilityLayout.Macro({ "CLOUDS_DEBUG_MARCHCOUNT" });

		list.BindPipeline(visibilityLayout);

		struct {
			uint32_t outputTexture;
			uint32_t weatherTexture;
			uint32_t baseShapeNoiseTexture;
			uint32_t curlShapeNoiseTexture;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			float solarZenithAngle;
			uint32_t timeSlice;
			uint32_t geometryDepthTexture;
			uint32_t blueNoiseTexture;
			uint32_t atmosphereIrradianceBuffer;
			float time;
			XMFLOAT2 wind;
			uint32_t upscaledResolution[2];
		} bindData;

		bindData.outputTexture = resources.Get(cloudVisibility);
		bindData.weatherTexture = resources.Get(weatherTag);
		bindData.baseShapeNoiseTexture = resources.Get(baseShapeNoiseTag);
		bindData.curlShapeNoiseTexture = resources.Get(curlShapeNoiseTag);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.timeSlice = Renderer::Get().GetAppFrame() % 16;
		bindData.geometryDepthTexture = resources.Get(depthStencil);
		bindData.blueNoiseTexture = resources.Get(blueNoiseTag);
		bindData.atmosphereIrradianceBuffer = resources.Get(atmosphereIrradiance);
		bindData.time = Renderer::Get().GetAppTime();
		bindData.wind = { windDirection.x * windStrength, windDirection.y * windStrength };
		bindData.upscaledResolution[0] = device->renderWidth;
		bindData.upscaledResolution[1] = device->renderHeight;

		list.BindConstants("bindData", bindData);

		const auto& outputComponent = device->GetResourceManager().Get(resources.GetTexture(cloudVisibility));
		const auto dispatchX = std::ceilf(outputComponent.description.width / 8.f);
		const auto dispatchY = std::ceilf(outputComponent.description.height / 8.f);

		list.Dispatch(dispatchX, dispatchY, 1);
	});

	auto& upscalePass = graph.AddPass("Clouds Upscale Pass", ExecutionQueue::Compute);
	const auto cloudOutputUpscaled = upscalePass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = 1.f,
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Clouds upscaled scattering transmittance"));
	const auto cloudDepthUpscaled = upscalePass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = 1.f,
		.format = DXGI_FORMAT_R32_FLOAT
	}, VGText("Clouds upscaled depth"));
	const auto cloudVisibilityUpscaled = upscalePass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = 1.f,
		.format = DXGI_FORMAT_R16G16_FLOAT
	}, VGText("Clouds upscaled sky visibility"));
	upscalePass.Read(cameraBuffer, ResourceBind::SRV);
	upscalePass.Read(depthStencil, ResourceBind::SRV);
	upscalePass.Read(cloudOutput, ResourceBind::SRV);
	upscalePass.Read(cloudDepth, ResourceBind::SRV);
	upscalePass.Read(cloudVisibility, ResourceBind::SRV);
	upscalePass.Read(lastFrameScatteringUpscaled, ResourceBind::SRV);
	upscalePass.Read(lastFrameDepthUpscaled, ResourceBind::SRV);
	upscalePass.Read(lastFrameVisibilityUpscaled, ResourceBind::SRV);
	upscalePass.Write(cloudOutputUpscaled, TextureView{}.UAV("", 0));
	upscalePass.Write(cloudDepthUpscaled, TextureView{}.UAV("", 0));
	upscalePass.Write(cloudVisibilityUpscaled, TextureView{}.UAV("", 0));
	upscalePass.Bind([this, cameraBuffer, depthStencil, cloudOutput, cloudDepth, cloudVisibility, oldUpscaled=lastFrameScatteringUpscaled,
		oldDepthUpscaled=lastFrameDepthUpscaled, oldVisibilityUpscaled=lastFrameVisibilityUpscaled, cloudOutputUpscaled,
		cloudDepthUpscaled, cloudVisibilityUpscaled](CommandList& list, RenderPassResources& resources)
	{
		auto upscaleLayout = RenderPipelineLayout{}
			.ComputeShader({ "Clouds/Upscale", "Main" });

		list.BindPipeline(upscaleLayout);

		struct {
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t timeSlice;
			uint32_t geometryDepthTexture;
			uint32_t newScatteringTransmittanceTexture;
			uint32_t newDepthTexture;
			uint32_t newVisibilityTexture;
			uint32_t oldScatteringTransmittanceTexture;
			uint32_t oldDepthTexture;
			uint32_t oldVisibilityTexture;
			uint32_t outputScatteringTransmittanceTexture;
			uint32_t outputDepthTexture;
			uint32_t outputVisibilityTexture;
		} bindData;

		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.timeSlice = Renderer::Get().GetAppFrame() % 16;
		bindData.geometryDepthTexture = resources.Get(depthStencil);
		bindData.newScatteringTransmittanceTexture = resources.Get(cloudOutput);
		bindData.newDepthTexture = resources.Get(cloudDepth);
		bindData.newVisibilityTexture = resources.Get(cloudVisibility);
		bindData.oldScatteringTransmittanceTexture = 0;
		if (oldUpscaled.id != 0)
			bindData.oldScatteringTransmittanceTexture = resources.Get(oldUpscaled);
		bindData.oldDepthTexture = 0;
		if (oldDepthUpscaled.id != 0)
			bindData.oldDepthTexture = resources.Get(oldDepthUpscaled);
		bindData.oldVisibilityTexture = 0;
		if (oldVisibilityUpscaled.id != 0)
			bindData.oldVisibilityTexture = resources.Get(oldVisibilityUpscaled);

		bindData.outputScatteringTransmittanceTexture = resources.Get(cloudOutputUpscaled);
		bindData.outputDepthTexture = resources.Get(cloudDepthUpscaled);
		bindData.outputVisibilityTexture = resources.Get(cloudVisibilityUpscaled);

		list.BindConstants("bindData", bindData);
		
		const auto& outputComponent = device->GetResourceManager().Get(resources.GetTexture(cloudOutputUpscaled));
		const auto dispatchX = std::ceilf(outputComponent.description.width / 8.f);
		const auto dispatchY = std::ceilf(outputComponent.description.height / 8.f);

		list.Dispatch(dispatchX, dispatchY, 1);
	});

	lastFrameScatteringUpscaled = cloudOutputUpscaled;
	lastFrameDepthUpscaled = cloudDepthUpscaled;
	lastFrameVisibilityUpscaled = cloudVisibilityUpscaled;

	return { cloudOutputUpscaled, cloudDepthUpscaled, cloudVisibilityUpscaled, cirrusTag, weatherTag };
}