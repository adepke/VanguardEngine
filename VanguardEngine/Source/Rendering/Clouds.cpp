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
	device->GetResourceManager().Destroy(environmentClouds);
}

void Clouds::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	CvarCreate("cloudRayMarchQuality", "Controls the ray march quality of the clouds. Increasing quality degrades performance. 0=lowDetail, 1=default, 2=groundTruth", 1);
	CvarCreate("cloudRenderScale", "Controls the render scale of the volumetric clouds", 0.25f);
	CvarCreate("cloudDebugVisualization", "Cloud debug visualisation: 0=off, 1=transmittance, 2=march count, 3=normal vector", 0);
	CvarCreate("cloudReflectionsEnabled", "Bake clouds into the IBL luminance cube so they appear in reflections", 1);
	CvarCreate("cloudReconstructionMode", "Cloud reconstruction: 0=legacy interleaved reprojection, 1=stochastic accumulation", 1);

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
	lastFrameAccumulatedScattering.id = 0;
	lastFrameAccumulatedDepth.id = 0;
	lastFrameAccumulatedVisibility.id = 0;
}

CloudResources Clouds::Render(RenderGraph& graph, entt::registry& registry, const Atmosphere& atmosphere, const RenderResource cameraBuffer, const RenderResource depthStencil, const RenderResource atmosphereIrradiance, const RenderResource luminanceTag)
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
		environmentBakeCounter = 0;  // Force a full bake.
	}

	auto& weatherPass = graph.AddPass("Weather Pass", ExecutionQueue::Compute);
	weatherPass.Write(weatherTag, TextureView{}.UAV("", 0));
	weatherPass.Bind([this, weatherTag](CommandList& list, RenderPassResources& resources)
	{
		GenerateWeather(list, resources.Get(weatherTag));
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
		environmentCloudsTag, solarZenithAngle, counter=environmentBakeCounter](CommandList& list, RenderPassResources& resources)
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
		bindData.wind = { windDirection.x * windStrength, windDirection.y * windStrength };
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
	const bool stochasticReconstruction = *CvarGet("cloudReconstructionMode", int) > 0;

	// Reset temporal history when the reconstruction mode changes, the history resources are
	// incompatible between modes (full res vs cloud render res).
	if ((stochasticReconstruction ? 1 : 0) != lastReconstructionMode)
	{
		lastFrameScatteringUpscaled.id = 0;
		lastFrameDepthUpscaled.id = 0;
		lastFrameVisibilityUpscaled.id = 0;
		lastFrameAccumulatedScattering.id = 0;
		lastFrameAccumulatedDepth.id = 0;
		lastFrameAccumulatedVisibility.id = 0;
		lastReconstructionMode = stochasticReconstruction ? 1 : 0;
	}

	// Downsample the geometry depth to the cloud render resolution, storing per-texel (min, max) raw
	// reversed-Z bounds. Min (the farthest depth) drives the conservative raymarch early-out and the
	// history occlusion tests; the full interval drives the bilateral upsample's similarity weights.
	auto& depthDownsamplePass = graph.AddPass("Clouds Geometry Depth Downsample Pass", ExecutionQueue::Compute);
	const auto geometryDepthMinMax = depthDownsamplePass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = cloudRenderScale,
		.format = DXGI_FORMAT_R32G32_FLOAT
	}, VGText("Clouds geometry depth min max"));
	depthDownsamplePass.Read(depthStencil, ResourceBind::SRV);
	depthDownsamplePass.Write(geometryDepthMinMax, TextureView{}.UAV("", 0));
	depthDownsamplePass.Bind([this, depthStencil, geometryDepthMinMax](CommandList& list, RenderPassResources& resources)
	{
		auto downsampleLayout = RenderPipelineLayout{}
			.ComputeShader({ "Clouds/DepthDownsample", "Main" });

		list.BindPipeline(downsampleLayout);

		struct {
			uint32_t depthTexture;
			uint32_t outputTexture;
		} bindData;

		bindData.depthTexture = resources.Get(depthStencil);
		bindData.outputTexture = resources.Get(geometryDepthMinMax);

		list.BindConstants("bindData", bindData);

		const auto& outputComponent = device->GetResourceManager().Get(resources.GetTexture(geometryDepthMinMax));
		const auto dispatchX = std::ceilf(outputComponent.description.width / 8.f);
		const auto dispatchY = std::ceilf(outputComponent.description.height / 8.f);

		list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, 1);
	});

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
	cloudsPass.Read(geometryDepthMinMax, ResourceBind::SRV);
	cloudsPass.Read(blueNoiseTag, ResourceBind::SRV);
	cloudsPass.Read(atmosphereIrradiance, ResourceBind::SRV);
	cloudsPass.Output(cloudOutput, OutputBind::RTV, LoadType::Preserve);
	cloudsPass.Write(cloudDepth, TextureView{}.UAV("", 0));
	cloudsPass.Bind([this, weatherTag, baseShapeNoiseTag, detailShapeNoiseTag, curlShapeNoiseTag, solarZenithAngle,
		cameraBuffer, geometryDepthMinMax, cloudOutput, blueNoiseTag, cloudDepth, atmosphereIrradiance]
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

		if (*CvarGet("cloudReconstructionMode", int) > 0)
			cloudsLayout.Macro({ "CLOUDS_STOCHASTIC" });

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
			uint32_t frameIndex;
			uint32_t depthTexture;
			uint32_t geometryDepthTexture;  // Downsampled (min, max) reversed-Z bounds.
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
		bindData.frameIndex = (uint32_t)Renderer::Get().GetAppFrame();  // Both jitter paths wrap internally.
		bindData.depthTexture = resources.Get(cloudDepth);
		bindData.geometryDepthTexture = resources.Get(geometryDepthMinMax);
		bindData.blueNoiseTexture = resources.Get(blueNoiseTag);
		bindData.atmosphereIrradianceBuffer = resources.Get(atmosphereIrradiance);
		bindData.time = Renderer::Get().GetAppTime();
		bindData.wind = { windDirection.x * windStrength, windDirection.y * windStrength };
		bindData.densityMultiplier = densityMultiplier;

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
	visibilityPass.Read(baseShapeNoiseTag, ResourceBind::SRV);  // Low detail marching.
	visibilityPass.Read(geometryDepthMinMax, ResourceBind::SRV);
	visibilityPass.Read(blueNoiseTag, ResourceBind::SRV);
	visibilityPass.Read(atmosphereIrradiance, ResourceBind::SRV);
	visibilityPass.Write(cloudVisibility, TextureView{}
		.UAV("", 0));
	visibilityPass.Bind([this, cameraBuffer, weatherTag, baseShapeNoiseTag, geometryDepthMinMax, blueNoiseTag, atmosphereIrradiance,
		cloudVisibility, solarZenithAngle](CommandList& list, RenderPassResources& resources)
	{
		auto visibilityLayout = RenderPipelineLayout{}
			.ComputeShader({ "Clouds/Visibility", "Main" })
			.Macro({ "CLOUDS_LOW_DETAIL" })  // Always low detail, no matter the quality setting
			.Macro({ "CLOUDS_MS_OCTAVES", 1 });  // No multi-scattering.

		if (*CvarGet("cloudReconstructionMode", int) > 0)
			visibilityLayout.Macro({ "CLOUDS_STOCHASTIC" });
			// Interestingly, applying the ONLY_DEPTH macro does not appear to help performance. The issue there is likely the transmittance
			// approximation being too conservative and allowing too many steps into the cloud. However, if this is done then small clouds
			// will yield too much shadow and does not look visibily correct.
			//.Macro({ "CLOUDS_ONLY_DEPTH" });
		
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
			uint32_t frameIndex;
			uint32_t geometryDepthTexture;  // Downsampled (min, max) reversed-Z bounds.
			uint32_t blueNoiseTexture;
			uint32_t atmosphereIrradianceBuffer;
			XMFLOAT2 wind;
			float time;
			uint32_t upscaledResolution[2];
		} bindData;

		bindData.outputTexture = resources.Get(cloudVisibility);
		bindData.weatherTexture = resources.Get(weatherTag);
		bindData.baseShapeNoiseTexture = resources.Get(baseShapeNoiseTag);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.frameIndex = (uint32_t)Renderer::Get().GetAppFrame();  // Both jitter paths wrap internally.
		bindData.geometryDepthTexture = resources.Get(geometryDepthMinMax);
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

	RenderResource cloudOutputUpscaled;
	RenderResource cloudDepthUpscaled;
	RenderResource cloudVisibilityUpscaled;

	if (stochasticReconstruction)
	{
		// Temporal accumulation at the cloud render resolution. Every low res pixel receives a fresh
		// stochastic sample every frame, so an exponential blend converges smoothly everywhere, unlike
		// the legacy interleaved reconstruction where each full res pixel refreshed once per 16 frames.
		auto& accumulatePass = graph.AddPass("Clouds Accumulation Pass", ExecutionQueue::Compute);
		const auto accumulatedScattering = accumulatePass.Create(TransientTextureDescription{
			.width = 0,
			.height = 0,
			.depth = 1,
			.resolutionScale = cloudRenderScale,
			.format = DXGI_FORMAT_R16G16B16A16_FLOAT
		}, VGText("Clouds accumulated scattering transmittance"));
		const auto accumulatedDepth = accumulatePass.Create(TransientTextureDescription{
			.width = 0,
			.height = 0,
			.depth = 1,
			.resolutionScale = cloudRenderScale,
			.format = DXGI_FORMAT_R32_FLOAT
		}, VGText("Clouds accumulated depth"));  // Inverse kilometers.
		const auto accumulatedVisibility = accumulatePass.Create(TransientTextureDescription{
			.width = 0,
			.height = 0,
			.depth = 1,
			.resolutionScale = cloudRenderScale,
			.format = DXGI_FORMAT_R16G16_FLOAT
		}, VGText("Clouds accumulated sky visibility"));
		accumulatePass.Read(cameraBuffer, ResourceBind::SRV);
		accumulatePass.Read(geometryDepthMinMax, ResourceBind::SRV);
		accumulatePass.Read(cloudOutput, ResourceBind::SRV);
		accumulatePass.Read(cloudDepth, ResourceBind::SRV);
		accumulatePass.Read(cloudVisibility, ResourceBind::SRV);
		accumulatePass.Read(lastFrameAccumulatedScattering, ResourceBind::SRV);
		accumulatePass.Read(lastFrameAccumulatedDepth, ResourceBind::SRV);
		accumulatePass.Read(lastFrameAccumulatedVisibility, ResourceBind::SRV);
		accumulatePass.Write(accumulatedScattering, TextureView{}.UAV("", 0));
		accumulatePass.Write(accumulatedDepth, TextureView{}.UAV("", 0));
		accumulatePass.Write(accumulatedVisibility, TextureView{}.UAV("", 0));
		accumulatePass.Bind([this, cameraBuffer, geometryDepthMinMax, cloudOutput, cloudDepth, cloudVisibility,
			historyScattering=lastFrameAccumulatedScattering, historyDepth=lastFrameAccumulatedDepth,
			historyVisibility=lastFrameAccumulatedVisibility, accumulatedScattering, accumulatedDepth,
			accumulatedVisibility](CommandList& list, RenderPassResources& resources)
		{
			auto accumulateLayout = RenderPipelineLayout{}
				.ComputeShader({ "Clouds/Accumulate", "Main" });

			list.BindPipeline(accumulateLayout);

			struct {
				uint32_t cameraBuffer;
				uint32_t cameraIndex;
				uint32_t geometryDepthMinMaxTexture;
				uint32_t newScatteringTransmittanceTexture;
				uint32_t newDepthTexture;
				uint32_t newVisibilityTexture;
				uint32_t historyScatteringTransmittanceTexture;
				uint32_t historyDepthTexture;
				uint32_t historyVisibilityTexture;
				uint32_t outputScatteringTransmittanceTexture;
				uint32_t outputDepthTexture;
				uint32_t outputVisibilityTexture;
			} bindData;

			bindData.cameraBuffer = resources.Get(cameraBuffer);
			bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
			bindData.geometryDepthMinMaxTexture = resources.Get(geometryDepthMinMax);
			bindData.newScatteringTransmittanceTexture = resources.Get(cloudOutput);
			bindData.newDepthTexture = resources.Get(cloudDepth);
			bindData.newVisibilityTexture = resources.Get(cloudVisibility);
			bindData.historyScatteringTransmittanceTexture = 0;
			if (historyScattering.id != 0)
				bindData.historyScatteringTransmittanceTexture = resources.Get(historyScattering);
			bindData.historyDepthTexture = 0;
			if (historyDepth.id != 0)
				bindData.historyDepthTexture = resources.Get(historyDepth);
			bindData.historyVisibilityTexture = 0;
			if (historyVisibility.id != 0)
				bindData.historyVisibilityTexture = resources.Get(historyVisibility);
			bindData.outputScatteringTransmittanceTexture = resources.Get(accumulatedScattering);
			bindData.outputDepthTexture = resources.Get(accumulatedDepth);
			bindData.outputVisibilityTexture = resources.Get(accumulatedVisibility);

			list.BindConstants("bindData", bindData);

			const auto& outputComponent = device->GetResourceManager().Get(resources.GetTexture(accumulatedScattering));
			const auto dispatchX = std::ceilf(outputComponent.description.width / 8.f);
			const auto dispatchY = std::ceilf(outputComponent.description.height / 8.f);

			list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, 1);
		});

		lastFrameAccumulatedScattering = accumulatedScattering;
		lastFrameAccumulatedDepth = accumulatedDepth;
		lastFrameAccumulatedVisibility = accumulatedVisibility;

		// Purely spatial bilateral upsample to full resolution, no full res history needed.
		auto& upsamplePass = graph.AddPass("Clouds Upsample Pass", ExecutionQueue::Compute);
		cloudOutputUpscaled = upsamplePass.Create(TransientTextureDescription{
			.width = 0,
			.height = 0,
			.depth = 1,
			.resolutionScale = 1.f,
			.format = DXGI_FORMAT_R16G16B16A16_FLOAT
		}, VGText("Clouds upscaled scattering transmittance"));
		cloudDepthUpscaled = upsamplePass.Create(TransientTextureDescription{
			.width = 0,
			.height = 0,
			.depth = 1,
			.resolutionScale = 1.f,
			.format = DXGI_FORMAT_R32_FLOAT
		}, VGText("Clouds upscaled depth"));
		cloudVisibilityUpscaled = upsamplePass.Create(TransientTextureDescription{
			.width = 0,
			.height = 0,
			.depth = 1,
			.resolutionScale = 1.f,
			.format = DXGI_FORMAT_R16G16_FLOAT
		}, VGText("Clouds upscaled sky visibility"));
		upsamplePass.Read(cameraBuffer, ResourceBind::SRV);
		upsamplePass.Read(depthStencil, ResourceBind::SRV);
		upsamplePass.Read(geometryDepthMinMax, ResourceBind::SRV);
		upsamplePass.Read(accumulatedScattering, ResourceBind::SRV);
		upsamplePass.Read(accumulatedDepth, ResourceBind::SRV);
		upsamplePass.Read(accumulatedVisibility, ResourceBind::SRV);
		upsamplePass.Write(cloudOutputUpscaled, TextureView{}.UAV("", 0));
		upsamplePass.Write(cloudDepthUpscaled, TextureView{}.UAV("", 0));
		upsamplePass.Write(cloudVisibilityUpscaled, TextureView{}.UAV("", 0));
		upsamplePass.Bind([this, cameraBuffer, depthStencil, geometryDepthMinMax, accumulatedScattering, accumulatedDepth,
			accumulatedVisibility, cloudOutputUpscaled, cloudDepthUpscaled, cloudVisibilityUpscaled]
			(CommandList& list, RenderPassResources& resources)
		{
			auto upsampleLayout = RenderPipelineLayout{}
				.ComputeShader({ "Clouds/Upsample", "Main" });

			list.BindPipeline(upsampleLayout);

			struct {
				uint32_t cameraBuffer;
				uint32_t cameraIndex;
				uint32_t geometryDepthTexture;
				uint32_t geometryDepthMinMaxTexture;
				uint32_t accumulatedScatteringTransmittanceTexture;
				uint32_t accumulatedDepthTexture;
				uint32_t accumulatedVisibilityTexture;
				uint32_t outputScatteringTransmittanceTexture;
				uint32_t outputDepthTexture;
				uint32_t outputVisibilityTexture;
			} bindData;

			bindData.cameraBuffer = resources.Get(cameraBuffer);
			bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
			bindData.geometryDepthTexture = resources.Get(depthStencil);
			bindData.geometryDepthMinMaxTexture = resources.Get(geometryDepthMinMax);
			bindData.accumulatedScatteringTransmittanceTexture = resources.Get(accumulatedScattering);
			bindData.accumulatedDepthTexture = resources.Get(accumulatedDepth);
			bindData.accumulatedVisibilityTexture = resources.Get(accumulatedVisibility);
			bindData.outputScatteringTransmittanceTexture = resources.Get(cloudOutputUpscaled);
			bindData.outputDepthTexture = resources.Get(cloudDepthUpscaled);
			bindData.outputVisibilityTexture = resources.Get(cloudVisibilityUpscaled);

			list.BindConstants("bindData", bindData);

			const auto& outputComponent = device->GetResourceManager().Get(resources.GetTexture(cloudOutputUpscaled));
			const auto dispatchX = std::ceilf(outputComponent.description.width / 8.f);
			const auto dispatchY = std::ceilf(outputComponent.description.height / 8.f);

			list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, 1);
		});
	}

	else
	{
		// Legacy interleaved reconstruction: full res history, jitter-aligned refresh.
		auto& upscalePass = graph.AddPass("Clouds Upscale Pass", ExecutionQueue::Compute);
		cloudOutputUpscaled = upscalePass.Create(TransientTextureDescription{
			.width = 0,
			.height = 0,
			.depth = 1,
			.resolutionScale = 1.f,
			.format = DXGI_FORMAT_R16G16B16A16_FLOAT
		}, VGText("Clouds upscaled scattering transmittance"));
		cloudDepthUpscaled = upscalePass.Create(TransientTextureDescription{
			.width = 0,
			.height = 0,
			.depth = 1,
			.resolutionScale = 1.f,
			.format = DXGI_FORMAT_R32_FLOAT
		}, VGText("Clouds upscaled depth"));
		cloudVisibilityUpscaled = upscalePass.Create(TransientTextureDescription{
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
	}

	return { cloudOutputUpscaled, cloudDepthUpscaled, cloudVisibilityUpscaled, cirrusTag, weatherTag };
}