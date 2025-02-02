// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/Clouds.h>
#include <Rendering/Device.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/Atmosphere.h>
#include <Rendering/RenderUtils.h>
#include <Utility/Math.h>

// #TEMP
#include <Rendering/Renderer.h>

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

void Clouds::GenerateNoise(CommandList& list, uint32_t baseShapeTexture, uint32_t detailShapeTexture)
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
}

Clouds::~Clouds()
{
	device->GetResourceManager().Destroy(baseShapeNoise);
	device->GetResourceManager().Destroy(detailShapeNoise);
}

void Clouds::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	CvarCreate("cloudShadowMapResolution", "Defines the width and height of the sun shadow map for clouds", 2048);
	CvarCreate("cloudShadowMapScale", "Multiplier for the scale of the cloud shadow map. Larger values increase scope but reduce fidelity", 0.05f);
	CvarCreate("cloudRayMarchQuality", "Controls the ray march quality of the clouds. Increasing quality degrades performance. 0=default, 1=groundTruth", 0);

	weatherLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Weather", "Main" });

	baseNoiseLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Shapes", "BaseShapeMain" });

	detailNoiseLayout = RenderPipelineLayout{}
		.ComputeShader({ "Clouds/Shapes", "DetailShapeMain" });

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

	//TextureDescription distortionNoiseDesc{
	//	.bindFlags = BindFlag::ShaderResource,
	//	.accessFlags = AccessFlag::GPUWrite,
	//	.width = 128,
	//	.height = 128,
	//	.depth = 1,
	//	.format = DXGI_FORMAT_R11G11B10_FLOAT
	//};
	//distortionNoise = device->GetResourceManager().Create(distortionNoiseDesc, VGText("Clouds distortion noise"));

	lastFrameClouds.id = 0;
}

CloudResources Clouds::Render(RenderGraph& graph, entt::registry& registry, const Atmosphere& atmosphere, const RenderResource cameraBuffer, const RenderResource depthStencil, const RenderResource atmosphereIrradiance)
{
	const auto weatherTag = graph.Import(weather);
	const auto baseShapeNoiseTag = graph.Import(baseShapeNoise);
	const auto detailShapeNoiseTag = graph.Import(detailShapeNoise);
	const auto solarZenithAngle = registry.get<TimeOfDayComponent>(atmosphere.sunLight).solarZenithAngle;
	const auto blueNoiseTag = graph.Import(RenderUtils::Get().blueNoise);

	if (dirty)
	{
		auto& noisePass = graph.AddPass("Clouds Noise Pass", ExecutionQueue::Compute);
		noisePass.Write(baseShapeNoiseTag, TextureView{}.UAV("", 0));
		noisePass.Write(detailShapeNoiseTag, TextureView{}.UAV("", 0));
		noisePass.Bind([this, baseShapeNoiseTag, detailShapeNoiseTag](CommandList& list, RenderPassResources& resources)
		{
			GenerateNoise(list, resources.Get(baseShapeNoiseTag), resources.Get(detailShapeNoiseTag));

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

	auto& cloudsPass = graph.AddPass("Clouds Pass", ExecutionQueue::Graphics);
	const auto cloudOutput = cloudsPass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = 1.f,
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT
	}, VGText("Clouds scattering transmittance"));
	cloudsPass.Read(cameraBuffer, ResourceBind::SRV);
	cloudsPass.Read(weatherTag, ResourceBind::SRV);
	cloudsPass.Read(baseShapeNoiseTag, ResourceBind::SRV);
	cloudsPass.Read(detailShapeNoiseTag, ResourceBind::SRV);
	cloudsPass.Read(depthStencil, ResourceBind::SRV);
	cloudsPass.Output(cloudOutput, OutputBind::RTV, LoadType::Preserve);
	cloudsPass.Read(lastFrameClouds, ResourceBind::SRV);
	cloudsPass.Read(blueNoiseTag, ResourceBind::SRV);
	cloudsPass.Read(atmosphereIrradiance, ResourceBind::SRV);
	const auto cloudDepth = cloudsPass.Create(TransientTextureDescription{
		.width = 0,
		.height = 0,
		.depth = 1,
		.resolutionScale = 1.f,
		.format = DXGI_FORMAT_R32_FLOAT
	}, VGText("Clouds depth"));
	cloudsPass.Write(cloudDepth, TextureView{}.UAV("", 0));
	cloudsPass.Bind([this, weatherTag, baseShapeNoiseTag, detailShapeNoiseTag, solarZenithAngle,
		cameraBuffer, depthStencil, cloudOutput, lastFrame=lastFrameClouds, blueNoiseTag, cloudDepth,
		atmosphereIrradiance](CommandList& list, RenderPassResources& resources)
	{
		auto cloudsLayout = RenderPipelineLayout{}
			.VertexShader({ "Clouds/Clouds", "VSMain" })
			.PixelShader({ "Clouds/Clouds", "PSMain" })
			.BlendMode(false, BlendMode{})
			.DepthEnabled(false);

		if (*CvarGet("cloudRayMarchQuality", int) > 0)
		{
			cloudsLayout.Macro({ "CLOUDS_MARCH_GROUND_TRUTH_DETAIL" });
		}

		list.BindPipeline(cloudsLayout);

		struct {
			uint32_t weatherTexture;
			uint32_t baseShapeNoiseTexture;
			uint32_t detailShapeNoiseTexture;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			float solarZenithAngle;
			uint32_t timeSlice;
			uint32_t lastFrameTexture;
			XMFLOAT2 outputResolution;
			uint32_t depthTexture;
			uint32_t geometryDepthTexture;
			uint32_t blueNoiseTexture;
			uint32_t atmosphereIrradianceBuffer;
			XMFLOAT2 wind;
			float time;
		} bindData;

		bindData.weatherTexture = resources.Get(weatherTag);
		bindData.baseShapeNoiseTexture = resources.Get(baseShapeNoiseTag);
		bindData.detailShapeNoiseTexture = resources.Get(detailShapeNoiseTag);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.solarZenithAngle = solarZenithAngle;

		static int time = 0;
		time++;
		bindData.timeSlice = time % 16;
		
		const auto& cloudOutputComponent = device->GetResourceManager().Get(resources.GetTexture(cloudOutput));
		bindData.outputResolution.x = cloudOutputComponent.description.width;
		bindData.outputResolution.y = cloudOutputComponent.description.height;

		bindData.lastFrameTexture = 0;
		if (lastFrame.id != 0)
			bindData.lastFrameTexture = resources.Get(lastFrame);

		bindData.depthTexture = resources.Get(cloudDepth);
		bindData.geometryDepthTexture = resources.Get(depthStencil);
		bindData.blueNoiseTexture = resources.Get(blueNoiseTag);
		bindData.atmosphereIrradianceBuffer = resources.Get(atmosphereIrradiance);
		bindData.wind = { windDirection.x * windStrength, windDirection.y * windStrength };
		bindData.time = Renderer::Get().GetAppTime();

		list.BindConstants("bindData", bindData);
		list.DrawFullscreenQuad();
	});

	auto& shadowPass = graph.AddPass("Clouds Shadow Map Pass", ExecutionQueue::Graphics);
	const auto shadowMapSize = *CvarGet("cloudShadowMapResolution", int);
	const auto shadowMapTag = shadowPass.Create(TransientTextureDescription{
		.width = (uint32_t)shadowMapSize,
		.height = (uint32_t)shadowMapSize,
		.depth = 1,
		.format = DXGI_FORMAT_R16_FLOAT
	}, VGText("Clouds shadow map"));
	shadowPass.Read(cameraBuffer, ResourceBind::SRV);
	shadowPass.Read(weatherTag, ResourceBind::SRV);
	shadowPass.Read(baseShapeNoiseTag, ResourceBind::SRV);
	shadowPass.Output(shadowMapTag, OutputBind::RTV, LoadType::Preserve);
	shadowPass.Bind([this, cameraBuffer, weatherTag, baseShapeNoiseTag, shadowMapTag, solarZenithAngle](CommandList& list, RenderPassResources& resources)
	{
		const auto orthographicScale = *CvarGet("cloudShadowMapResolution", int) * *CvarGet("cloudShadowMapScale", float);
		auto shadowMapLayout = RenderPipelineLayout{}
			.VertexShader({ "Clouds/Clouds", "VSMain" })
			.PixelShader({ "Clouds/Clouds", "PSMain" })
			.BlendMode(false, BlendMode{})
			.DepthEnabled(false)
			.Macro({ "CLOUDS_LOW_DETAIL" })
			.Macro({ "CLOUDS_FULL_RESOLUTION" })
			.Macro({ "CLOUDS_ONLY_DEPTH" })
			.Macro({ "CLOUDS_RENDER_ORTHOGRAPHIC" })
			.Macro({ "CLOUDS_CAMERA_IN_KILOMETERS" })
			.Macro({ "CLOUDS_ORTHOGRAPHIC_SCALE", orthographicScale });  // Scale is in kilometers.

		if (*CvarGet("cloudRayMarchQuality", int) > 0)
		{
			shadowMapLayout.Macro({ "CLOUDS_MARCH_GROUND_TRUTH_DETAIL" });
		}

		list.BindPipeline(shadowMapLayout);

		struct {
			uint32_t weatherTexture;
			uint32_t baseShapeNoiseTexture;
			uint32_t detailShapeNoiseTexture;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			float solarZenithAngle;
			uint32_t timeSlice;
			uint32_t lastFrameTexture;
			XMFLOAT2 outputResolution;
			uint32_t depthTexture;
			uint32_t geometryDepthTexture;
			uint32_t blueNoiseTexture;
			uint32_t atmosphereIrradianceBuffer;
			XMFLOAT2 wind;
			float time;
		} bindData;

		bindData.weatherTexture = resources.Get(weatherTag);
		bindData.baseShapeNoiseTexture = resources.Get(baseShapeNoiseTag);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 2;  // #TODO: This is awful.
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.wind = { windDirection.x * windStrength, windDirection.y * windStrength };
		bindData.time = Renderer::Get().GetAppTime();

		list.BindConstants("bindData", bindData);
		list.DrawFullscreenQuad();
	});

	lastFrameClouds = cloudOutput;

	return { cloudOutput, cloudDepth, shadowMapTag, weatherTag };
}