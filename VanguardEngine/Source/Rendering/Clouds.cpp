// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/Clouds.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/Atmosphere.h>
#include <Rendering/AccelerationStructures.h>
#include <Rendering/RenderUtils.h>
#include <Asset/TextureLoader.h>
#include <Core/Config.h>
#include <Utility/Math.h>

void Clouds::GenerateWeather(CommandList& list, uint32_t weatherTexture, const WeatherComponent& weather)
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
	bindData.globalCoverage = weather.coverage;
	bindData.precipitation = weather.precipitation;
	bindData.time = Renderer::Get().GetAppTime();
	bindData.wind = { weather.windDirection.x * weather.windStrength, weather.windDirection.y * weather.windStrength };

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
	device->GetResourceManager().Destroy(environmentClouds);
}

void Clouds::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	CvarCreate("cloudRayMarchQuality", "Controls the ray march quality of the clouds. Increasing quality degrades performance. 0=lowDetail, 1=default, 2=groundTruth", 1);
	CvarCreate("cloudRenderScale", "Controls the render scale of the volumetric clouds", 0.25f);
	CvarCreate("cloudDebugVisualization", "Cloud debug visualisation: 0=off, 1=transmittance, 2=march count, 3=normal vector", 0);
	CvarCreate("cloudReflectionsEnabled", "Bake clouds into the IBL luminance cube so they appear in reflections", 1);
	CvarCreate("atmosphereVisibilityContributeGeometry", "Geometry casts volumetric light shafts. 0=off, 1=on", 1);
	CvarCreate("atmosphereVisibilityContributeClouds", "Clouds cast volumetric light shafts. 0=off, 1=on", 1);

	weatherLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Weather", "Main" });

	baseNoiseLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Shapes", "BaseShapeMain" });

	detailNoiseLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Shapes", "DetailShapeMain" });

	curlNoiseLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Shapes", "CurlNoiseMain" });

	environmentBakeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/EnvironmentBake", "Main" })
		.Macro({ "CLOUDS_LOW_DETAIL" })
		.Macro({ "CLOUDS_MS_OCTAVES", 1 });

	environmentCompositeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/EnvironmentBake", "CompositeMain" })
		.Macro({ "CLOUDS_LOW_DETAIL" })  // Use same macros as environmentBakeLayout
		.Macro({ "CLOUDS_MS_OCTAVES", 1 });

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

	TextureDescription environmentCloudsDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = environmentCloudsSize,
		.height = environmentCloudsSize,
		.depth = 6,  // Texture cube.
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.array = true
	};
	environmentClouds = device->GetResourceManager().Create(environmentCloudsDesc, VGText("Clouds environment cube"));

	cirrusClouds = AssetLoader::LoadTexture(*device, Config::utilitiesPath / "Cirrus4k.png", false);

	lastFrameScatteringUpscaled.id = 0;
	lastFrameDepthUpscaled.id = 0;
	lastFrameVisibilityUpscaled.id = 0;
}

CloudResources Clouds::Render(RenderGraph& graph, entt::registry& registry, const Atmosphere& atmosphere,
	const RenderResource cameraBuffer, const RenderResource depthStencil, const RenderResource atmosphereIrradiance,
	const RenderResource luminanceTag, const AccelerationStructureResources& asResources)
{
	const auto weatherTag = graph.Import(weather);
	const auto baseShapeNoiseTag = graph.Import(baseShapeNoise);
	const auto detailShapeNoiseTag = graph.Import(detailShapeNoise);
	const auto curlShapeNoiseTag = graph.Import(curlShapeNoise);
	const auto cirrusTag = graph.Import(cirrusClouds);
	const auto blueNoiseTag = graph.Import(RenderUtils::Get().blueNoise);

	WeatherComponent weatherComp{};
	registry.view<const WeatherComponent>().each([&weatherComp](auto entity, const auto& weather)
	{
		weatherComp = weather;
	});
	const XMFLOAT2 wind = { weatherComp.windDirection.x * weatherComp.windStrength, weatherComp.windDirection.y * weatherComp.windStrength };

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
		environmentBakeCounter = 0;  // Force a full bake.
	}

	auto& weatherPass = graph.AddPass("Weather Pass", ExecutionQueue::Compute);
	weatherPass.Write(weatherTag, TextureView{}.UAV("", 0));
	weatherPass.Bind([this, weatherTag, weatherComp](CommandList& list, RenderPassResources& resources)
	{
		GenerateWeather(list, resources.Get(weatherTag), weatherComp);
	});

	// Bind data is shared in both the bake and composite passes.
	struct EnvironmentBindData
	{
		uint32_t luminanceTexture;
		uint32_t environmentCloudsTexture;
		uint32_t weatherTexture;
		uint32_t baseShapeNoiseTexture;
		uint32_t atmosphereIrradianceBuffer;
		uint32_t cameraBuffer;
		uint32_t cameraIndex;
		float solarZenithAngle;
		XMFLOAT2 wind;
		float time;
		float densityMultiplier;
		uint32_t baseFace;
		uint32_t jitterFrame;
		float blendFactor;
	};

	const bool bakeEnabled = *CvarGet("cloudReflectionsEnabled", int) > 0;
	const uint32_t luminanceSize = atmosphere.GetLuminanceTextureSize();
	const auto environmentCloudsTag = graph.Import(environmentClouds);

	auto& environmentBakePass = graph.AddPass("Clouds Environment Bake Pass", ExecutionQueue::Compute, bakeEnabled);
	environmentBakePass.Read(cameraBuffer, ResourceBind::SRV);
	environmentBakePass.Read(weatherTag, ResourceBind::SRV);
	environmentBakePass.Read(baseShapeNoiseTag, ResourceBind::SRV);  // Low detail marching.
	environmentBakePass.Read(atmosphereIrradiance, ResourceBind::SRV);
	environmentBakePass.Write(environmentCloudsTag, TextureView{}.UAV("", 0));
	environmentBakePass.Bind([this, cameraBuffer, weatherTag, baseShapeNoiseTag, curlShapeNoiseTag, atmosphereIrradiance,
		environmentCloudsTag, solarZenithAngle, wind, counter=environmentBakeCounter](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipeline(environmentBakeLayout);

		// A full bake happens when we don't have history to reuse, so render all faces with no blending in one frame.
		bool fullBake = counter == 0;

		EnvironmentBindData bindData{};
		bindData.environmentCloudsTexture = resources.Get(environmentCloudsTag);
		bindData.weatherTexture = resources.Get(weatherTag);
		bindData.baseShapeNoiseTexture = resources.Get(baseShapeNoiseTag);
		bindData.atmosphereIrradianceBuffer = resources.Get(atmosphereIrradiance);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.time = Renderer::Get().GetAppTime();
		bindData.wind = wind;
		bindData.densityMultiplier = densityMultiplier;
		bindData.baseFace = counter % 6;
		bindData.jitterFrame = counter;
		bindData.blendFactor = fullBake ? 1.f : environmentBlendFactor;

		list.BindConstants("bindData", bindData);

		list.Dispatch(environmentCloudsSize / 8, environmentCloudsSize / 8, fullBake ? 6 : 1);
	});

	environmentBakeCounter++;

	// Composite the cloud environment cube on top of the sky luminance cube. The two cubes are separated to allow proper
	// temporal accumulation of the cloud cube, previous implementation using one-shot per face needed too many raymarches to
	// get decent quality. Note this pass must run before IBL convolution.
	auto& environmentCompositePass = graph.AddPass("Clouds Environment Composite Pass", ExecutionQueue::Compute, bakeEnabled);
	environmentCompositePass.Read(environmentCloudsTag, ResourceBind::SRV);
	environmentCompositePass.Write(luminanceTag, TextureView{}.UAV("", 0));
	environmentCompositePass.Bind([this, environmentCloudsTag, luminanceTag, luminanceSize](CommandList& list, RenderPassResources& resources)
	{
		list.BindPipeline(environmentCompositeLayout);

		EnvironmentBindData bindData{};
		bindData.luminanceTexture = resources.Get(luminanceTag);
		bindData.environmentCloudsTexture = resources.Get(environmentCloudsTag);

		list.BindConstants("bindData", bindData);

		list.Dispatch(luminanceSize / 8, luminanceSize / 8, 6);
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
	cloudsPass.Bind([this, weatherTag, baseShapeNoiseTag, detailShapeNoiseTag, curlShapeNoiseTag, solarZenithAngle, wind,
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
		
		cloudsLayout.Macro({ "CLOUDS_MS_OCTAVES", msOctaves });

		const int cloudDebugMode = *CvarGet("cloudDebugVisualization", int);
		if (cloudDebugMode == 1)
			cloudsLayout.Macro({ "CLOUDS_DEBUG_TRANSMITTANCE" });
		else if (cloudDebugMode == 2)
			cloudsLayout.Macro({ "CLOUDS_DEBUG_MARCHCOUNT" });
		else if (cloudDebugMode == 3)
			cloudsLayout.Macro({ "CLOUDS_DEBUG_NORMALVECTOR" });

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
			float densityMultiplier;
			float padding;
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
		bindData.wind = wind;
		bindData.densityMultiplier = densityMultiplier;

		const auto& cloudOutputComponent = device->GetResourceManager().Get(resources.GetTexture(cloudOutput));
		bindData.outputResolution[0] = cloudOutputComponent.description.width;
		bindData.outputResolution[1] = cloudOutputComponent.description.height;

		bindData.upscaledResolution[0] = device->renderWidth;
		bindData.upscaledResolution[1] = device->renderHeight;

		list.BindConstants("bindData", bindData);
		list.DrawFullscreenQuad();
	});

	const bool geometryContribution = asResources.valid && *CvarGet("atmosphereVisibilityContributeGeometry", int) > 0;
	const bool cloudsContribution = *CvarGet("atmosphereVisibilityContributeClouds", int) > 0;

	auto& visibilityPass = graph.AddPass("Clouds Sky Visibility Pass", ExecutionQueue::Compute, *CvarGet("atmosphereVisibility", int) > 0);
	// #TODO: rename visibility and consider moving to atmosphere instead.
	const auto cloudVisibility = visibilityPass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = cloudRenderScale,
		.format = DXGI_FORMAT_R16G16_FLOAT
	}, VGText("Clouds visibility map"));
	visibilityPass.Read(cameraBuffer, ResourceBind::SRV);
	visibilityPass.Read(depthStencil, ResourceBind::SRV);
	visibilityPass.Read(blueNoiseTag, ResourceBind::SRV);
	if (cloudsContribution)
	{
		visibilityPass.Read(weatherTag, ResourceBind::SRV);
		visibilityPass.Read(baseShapeNoiseTag, ResourceBind::SRV);  // Low detail marching.
		visibilityPass.Read(atmosphereIrradiance, ResourceBind::SRV);
	}
	if (geometryContribution)
	{
		visibilityPass.Read(asResources.tlasTag, ResourceBind::AS);
	}
	visibilityPass.Write(cloudVisibility, TextureView{}
		.UAV("", 0));
	visibilityPass.Bind([this, geometryContribution, cloudsContribution, cameraBuffer, weatherTag, baseShapeNoiseTag,
		depthStencil, blueNoiseTag, atmosphereIrradiance, cloudVisibility, solarZenithAngle, wind, asResources]
		(CommandList& list, RenderPassResources& resources)
	{
		auto visibilityLayout = RenderPipelineLayout{}
			.ComputeShader({ "Clouds/Visibility", "Main" })
			.Macro({ "CLOUDS_LOW_DETAIL" })  // Always low detail, no matter the quality setting
			.Macro({ "CLOUDS_MS_OCTAVES", 1 });  // No multi-scattering.
			// Interestingly, applying the ONLY_DEPTH macro does not appear to help performance. The issue there is likely the transmittance
			// approximation being too conservative and allowing too many steps into the cloud. However, if this is done then small clouds
			// will yield too much shadow and does not look visibily correct.
			//.Macro({ "CLOUDS_ONLY_DEPTH" });
		
		if (geometryContribution)
			visibilityLayout.Macro({ "VISIBILITY_ENABLE_GEOMETRY" });
		if (cloudsContribution)
			visibilityLayout.Macro({ "VISIBILITY_ENABLE_CLOUDS" });

		// Only really useful with an inspector attached, no visual cue of the step count here.
		if (*CvarGet("cloudDebugVisualization", int) == 2)
			visibilityLayout.Macro({ "CLOUDS_DEBUG_MARCHCOUNT" });

		list.BindPipeline(visibilityLayout);

		struct {
			uint32_t outputTexture;
			uint32_t weatherTexture;
			uint32_t baseShapeNoiseTexture;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			float solarZenithAngle;
			uint32_t timeSlice;
			uint32_t geometryDepthTexture;
			uint32_t blueNoiseTexture;
			uint32_t atmosphereIrradianceBuffer;
			XMFLOAT2 wind;
			float time;
			uint32_t upscaledResolution[2];
			uint32_t accelerationStructure;
		} bindData;

		bindData.outputTexture = resources.Get(cloudVisibility);
		bindData.weatherTexture = cloudsContribution ? resources.Get(weatherTag) : 0;
		bindData.baseShapeNoiseTexture = cloudsContribution ? resources.Get(baseShapeNoiseTag) : 0;
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.timeSlice = Renderer::Get().GetAppFrame() % 16;
		bindData.geometryDepthTexture = resources.Get(depthStencil);
		bindData.blueNoiseTexture = resources.Get(blueNoiseTag);
		bindData.atmosphereIrradianceBuffer = cloudsContribution ? resources.Get(atmosphereIrradiance) : 0;
		bindData.time = Renderer::Get().GetAppTime();
		bindData.wind = wind;
		bindData.upscaledResolution[0] = device->renderWidth;
		bindData.upscaledResolution[1] = device->renderHeight;
		bindData.accelerationStructure = geometryContribution ? resources.Get(asResources.tlasTag) : 0;

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