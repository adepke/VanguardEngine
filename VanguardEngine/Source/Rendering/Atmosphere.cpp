// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/Atmosphere.h>
#include <Rendering/Device.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/ShaderStructs.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>

#include <vector>
#include <cmath>

void Atmosphere::Precompute(CommandList& list, TextureHandle transmittanceHandle, TextureHandle scatteringHandle, TextureHandle irradianceHandle)
{
	constexpr auto groupSize = 8;
	constexpr auto scatteringOrder = 4;

	const auto& transmittanceComponent = device->GetResourceManager().Get(transmittanceHandle);
	const auto& scatteringComponent = device->GetResourceManager().Get(scatteringHandle);
	const auto& irradianceComponent = device->GetResourceManager().Get(irradianceHandle);
	const auto& deltaRayleighComponent = device->GetResourceManager().Get(deltaRayleighTexture);
	const auto& deltaMieComponent = device->GetResourceManager().Get(deltaMieTexture);
	const auto& deltaScatteringDensityComponent = device->GetResourceManager().Get(deltaScatteringDensityTexture);
	const auto& deltaIrradianceComponent = device->GetResourceManager().Get(deltaIrradianceTexture);

	struct BindData
	{
		AtmosphereData atmosphere;
		uint32_t transmissionTexture;
		uint32_t scatteringTexture;
		uint32_t irradianceTexture;
		uint32_t deltaRayleighTexture;
		uint32_t deltaMieTexture;
		uint32_t deltaScatteringDensityTexture;
		uint32_t deltaIrradianceTexture;
		int32_t scatteringOrder;
	} bindData;

	bindData.atmosphere = model;

	auto transmissionUAV = device->AllocateDescriptor(DescriptorType::Default);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = transmittanceComponent.description.format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;
	device->Native()->CreateUnorderedAccessView(transmittanceComponent.allocation->GetResource(), nullptr, &uavDesc, transmissionUAV);

	auto scatteringUAV = device->AllocateDescriptor(DescriptorType::Default);
	uavDesc.Format = scatteringComponent.description.format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = scatteringComponent.description.depth;
	device->Native()->CreateUnorderedAccessView(scatteringComponent.allocation->GetResource(), nullptr, &uavDesc, scatteringUAV);

	auto irradianceUAV = device->AllocateDescriptor(DescriptorType::Default);
	uavDesc.Format = irradianceComponent.description.format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;
	device->Native()->CreateUnorderedAccessView(irradianceComponent.allocation->GetResource(), nullptr, &uavDesc, irradianceUAV);

	auto deltaRayleighUAV = device->AllocateDescriptor(DescriptorType::Default);
	uavDesc.Format = deltaRayleighComponent.description.format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = deltaRayleighComponent.description.depth;
	device->Native()->CreateUnorderedAccessView(deltaRayleighComponent.allocation->GetResource(), nullptr, &uavDesc, deltaRayleighUAV);

	auto deltaMieUAV = device->AllocateDescriptor(DescriptorType::Default);
	uavDesc.Format = deltaMieComponent.description.format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = deltaMieComponent.description.depth;
	device->Native()->CreateUnorderedAccessView(deltaMieComponent.allocation->GetResource(), nullptr, &uavDesc, deltaMieUAV);

	auto deltaScatteringDensityUAV = device->AllocateDescriptor(DescriptorType::Default);
	uavDesc.Format = deltaScatteringDensityComponent.description.format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = deltaScatteringDensityComponent.description.depth;
	device->Native()->CreateUnorderedAccessView(deltaScatteringDensityComponent.allocation->GetResource(), nullptr, &uavDesc, deltaScatteringDensityUAV);

	auto deltaIrradianceUAV = device->AllocateDescriptor(DescriptorType::Default);
	uavDesc.Format = deltaIrradianceComponent.description.format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;
	device->Native()->CreateUnorderedAccessView(deltaIrradianceComponent.allocation->GetResource(), nullptr, &uavDesc, deltaIrradianceUAV);

	// Transmission.

	auto dispatchX = std::ceil((float)transmittanceComponent.description.width / groupSize);
	auto dispatchY = std::ceil((float)transmittanceComponent.description.height / groupSize);
	auto dispatchZ = 1.f;

	bindData.transmissionTexture = transmissionUAV.bindlessIndex;
	bindData.scatteringTexture = 0;
	bindData.irradianceTexture = 0;
	bindData.deltaRayleighTexture = 0;
	bindData.deltaMieTexture = 0;
	bindData.deltaScatteringDensityTexture = 0;
	bindData.deltaIrradianceTexture = 0;

	list.TransitionBarrier(transmittanceHandle, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.FlushBarriers();

	list.BindPipeline(transmissionPrecomputeLayout);
	list.BindDescriptorAllocator(device->GetDescriptorAllocator());
	list.BindConstants("bindData", bindData);
	list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
	list.UAVBarrier(transmittanceHandle);

	// Direct irradiance.

	dispatchX = std::ceil((float)irradianceComponent.description.width / groupSize);
	dispatchY = std::ceil((float)irradianceComponent.description.height / groupSize);
	dispatchZ = 1.f;

	bindData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
	bindData.scatteringTexture = 0;
	bindData.irradianceTexture = irradianceUAV.bindlessIndex;
	bindData.deltaRayleighTexture = 0;
	bindData.deltaMieTexture = 0;
	bindData.deltaScatteringDensityTexture = 0;
	bindData.deltaIrradianceTexture = deltaIrradianceUAV.bindlessIndex;

	list.TransitionBarrier(transmittanceHandle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list.TransitionBarrier(irradianceHandle, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.TransitionBarrier(deltaIrradianceTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.FlushBarriers();

	list.BindPipeline(directIrradiancePrecomputeLayout);
	list.BindDescriptorAllocator(device->GetDescriptorAllocator());
	list.BindConstants("bindData", bindData);
	list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
	list.UAVBarrier(deltaIrradianceTexture);
	list.UAVBarrier(irradianceHandle);

	// Single scattering.

	dispatchX = std::ceil((float)scatteringComponent.description.width / groupSize);
	dispatchY = std::ceil((float)scatteringComponent.description.height / groupSize);
	dispatchZ = std::ceil((float)scatteringComponent.description.depth / 1.f);

	bindData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
	bindData.scatteringTexture = scatteringUAV.bindlessIndex;
	bindData.irradianceTexture = 0;
	bindData.deltaRayleighTexture = deltaRayleighUAV.bindlessIndex;
	bindData.deltaMieTexture = deltaMieUAV.bindlessIndex;
	bindData.deltaScatteringDensityTexture = 0;
	bindData.deltaIrradianceTexture = 0;

	list.TransitionBarrier(transmittanceHandle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list.TransitionBarrier(scatteringHandle, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.TransitionBarrier(deltaRayleighTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.TransitionBarrier(deltaMieTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.FlushBarriers();

	list.BindPipeline(singleScatteringPrecomputeLayout);
	list.BindDescriptorAllocator(device->GetDescriptorAllocator());
	list.BindConstants("bindData", bindData);
	list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
	list.UAVBarrier(deltaRayleighTexture);
	list.UAVBarrier(deltaMieTexture);
	list.UAVBarrier(scatteringHandle);

	for (int i = 2; i <= scatteringOrder; ++i)
	{
		std::string zoneName = "Precompute scattering order " + std::to_string(i);
		VGScopedGPUTransientStat(zoneName.c_str(), device->GetDirectContext(), list.Native());

		// Scattering density.

		dispatchX = std::ceil((float)deltaScatteringDensityComponent.description.width / groupSize);
		dispatchY = std::ceil((float)deltaScatteringDensityComponent.description.height / groupSize);
		dispatchZ = std::ceil((float)deltaScatteringDensityComponent.description.depth / 1.f);

		bindData.scatteringOrder = i;
		bindData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
		bindData.scatteringTexture = 0;
		bindData.irradianceTexture = 0;
		bindData.deltaRayleighTexture = deltaRayleighComponent.SRV->bindlessIndex;
		bindData.deltaMieTexture = deltaMieComponent.SRV->bindlessIndex;
		bindData.deltaScatteringDensityTexture = deltaScatteringDensityUAV.bindlessIndex;
		bindData.deltaIrradianceTexture = deltaIrradianceComponent.SRV->bindlessIndex;

		list.TransitionBarrier(transmittanceHandle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaRayleighTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaMieTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaScatteringDensityTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.TransitionBarrier(deltaIrradianceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.FlushBarriers();

		list.BindPipeline(scatteringDensityPrecomputeLayout);
		list.BindDescriptorAllocator(device->GetDescriptorAllocator());
		list.BindConstants("bindData", bindData);
		list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
		list.UAVBarrier(deltaScatteringDensityTexture);

		// Indirect irradiance.

		dispatchX = std::ceil((float)irradianceComponent.description.width / groupSize);
		dispatchY = std::ceil((float)irradianceComponent.description.height / groupSize);
		dispatchZ = 1.f;

		bindData.scatteringOrder = i - 1;
		bindData.transmissionTexture = 0;
		bindData.scatteringTexture = 0;
		bindData.irradianceTexture = irradianceUAV.bindlessIndex;
		bindData.deltaRayleighTexture = deltaRayleighComponent.SRV->bindlessIndex;
		bindData.deltaMieTexture = deltaMieComponent.SRV->bindlessIndex;
		bindData.deltaScatteringDensityTexture = 0;
		bindData.deltaIrradianceTexture = deltaIrradianceUAV.bindlessIndex;

		list.TransitionBarrier(irradianceHandle, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.TransitionBarrier(deltaRayleighTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaMieTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaIrradianceTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.FlushBarriers();

		list.BindPipeline(indirectIrradiancePrecomputeLayout);
		list.BindDescriptorAllocator(device->GetDescriptorAllocator());
		list.BindConstants("bindData", bindData);
		list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
		list.UAVBarrier(deltaIrradianceTexture);
		list.UAVBarrier(irradianceHandle);

		// Multiple scattering.

		dispatchX = std::ceil((float)scatteringComponent.description.width / groupSize);
		dispatchY = std::ceil((float)scatteringComponent.description.height / groupSize);
		dispatchZ = std::ceil((float)scatteringComponent.description.depth / 1.f);

		bindData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
		bindData.scatteringTexture = scatteringUAV.bindlessIndex;
		bindData.irradianceTexture = 0;
		bindData.deltaRayleighTexture = deltaRayleighUAV.bindlessIndex;
		bindData.deltaMieTexture = 0;
		bindData.deltaScatteringDensityTexture = deltaScatteringDensityComponent.SRV->bindlessIndex;
		bindData.deltaIrradianceTexture = 0;

		list.TransitionBarrier(transmittanceHandle, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(scatteringHandle, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.TransitionBarrier(deltaRayleighTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.TransitionBarrier(deltaScatteringDensityTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.FlushBarriers();

		list.BindPipeline(multipleScatteringPrecomputeLayout);
		list.BindDescriptorAllocator(device->GetDescriptorAllocator());
		list.BindConstants("bindData", bindData);
		list.Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
		list.UAVBarrier(deltaRayleighTexture);
		list.UAVBarrier(scatteringHandle);
	}

	// UAV cleanup.

	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(transmissionUAV));
	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(scatteringUAV));
	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(irradianceUAV));
	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(deltaRayleighUAV));
	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(deltaMieUAV));
	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(deltaScatteringDensityUAV));
	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(deltaIrradianceUAV));
}

Atmosphere::~Atmosphere()
{
	device->GetResourceManager().Destroy(transmittanceTexture);
	device->GetResourceManager().Destroy(scatteringTexture);
	device->GetResourceManager().Destroy(irradianceTexture);
	device->GetResourceManager().Destroy(deltaRayleighTexture);
	device->GetResourceManager().Destroy(deltaMieTexture);
	device->GetResourceManager().Destroy(deltaScatteringDensityTexture);
	device->GetResourceManager().Destroy(deltaIrradianceTexture);
	device->GetResourceManager().Destroy(luminanceTexture);
}

void Atmosphere::Initialize(RenderDevice* inDevice, entt::registry& registry)
{
	device = inDevice;

	CvarCreate("renderCloudShadowMap", "Projects the cloud shadow map onto the planet surface, for debugging purposes. 0=off, 1=on", 0);
	CvarCreate("renderLightShafts", "Controls rendering of volumetric light shafts, currently only cast by clouds. 0=off, 1=on", 1);

	transmissionPrecomputeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Atmosphere/AtmospherePrecompute", "TransmittanceLutMain" });

	directIrradiancePrecomputeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Atmosphere/AtmospherePrecompute", "DirectIrradianceLutMain" });

	singleScatteringPrecomputeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Atmosphere/AtmospherePrecompute", "SingleScatteringLutMain" });

	scatteringDensityPrecomputeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Atmosphere/AtmospherePrecompute", "ScatteringDensityLutMain" });

	indirectIrradiancePrecomputeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Atmosphere/AtmospherePrecompute", "IndirectIrradianceLutMain" });

	multipleScatteringPrecomputeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Atmosphere/AtmospherePrecompute", "MultipleScatteringLutMain" });

	separableIrradianceLayout = RenderPipelineLayout{}
		.ComputeShader({ "Atmosphere/SeparableIrradiance", "Main" });

	luminancePrecomputeLayout = RenderPipelineLayout{}
		.ComputeShader({ "Atmosphere/Luminance", "Main" });

	TextureDescription transmittanceDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = 256,
		.height = 64,
		.depth = 1,
		.format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		.mipMapping = false
	};

	transmittanceTexture = device->GetResourceManager().Create(transmittanceDesc, VGText("Atmosphere precomputed transmittance"));

	TextureDescription scatteringDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = 256,
		.height = 128,
		.depth = 32,
		.format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		.mipMapping = false
	};

	scatteringTexture = device->GetResourceManager().Create(scatteringDesc, VGText("Atmosphere precomputed scattering"));

	TextureDescription irradianceDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = 64,
		.height = 16,
		.depth = 1,
		.format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		.mipMapping = false
	};

	irradianceTexture = device->GetResourceManager().Create(irradianceDesc, VGText("Atmosphere precomputed irradiance"));

	deltaRayleighTexture = device->GetResourceManager().Create(scatteringDesc, VGText("Atmosphere delta rayleigh"));
	deltaMieTexture = device->GetResourceManager().Create(scatteringDesc, VGText("Atmosphere delta mie"));
	deltaScatteringDensityTexture = device->GetResourceManager().Create(scatteringDesc, VGText("Atmosphere delta scattering density"));
	deltaIrradianceTexture = device->GetResourceManager().Create(irradianceDesc, VGText("Atmosphere delta irradiance"));

	TextureDescription luminanceDesc{
		.bindFlags = BindFlag::ShaderResource | BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.width = luminanceTextureSize,
		.height = luminanceTextureSize,
		.depth = 6,  // Texture cube.
		.format = DXGI_FORMAT_R16G16B16A16_FLOAT,
		.mipMapping = true,
		.array = true
	};

	luminanceTexture = device->GetResourceManager().Create(luminanceDesc, VGText("Atmosphere luminance"));

	XMFLOAT3 rayleighScattering = { 0.005802f, 0.013558f, 0.0331f };
	float mieScattering = 0.003996f * 1.2f;
	float mieExtinction = 1.11f * mieScattering;
	XMFLOAT3 ozoneAbsorption = { 0.0020556f, 0.0049788f, 0.0002136f };  // Frostbite's.
	//XMFLOAT3 ozoneAbsorption = { 0.00065f, 0.001881f, 0.000085f };  // Bruneton's.

	model.radiusBottom = 6360.f;  // Kilometers.
	model.radiusTop = 6420.f;  // Kilometers.
	model.rayleighDensity.width = 0.f;
	model.rayleighDensity.exponentialCoefficient = 1.f;
	model.rayleighDensity.exponentialScale = -1.f / 8.f;
	model.rayleighDensity.heightScale = 0.f;
	model.rayleighDensity.offset = 0.f;
	model.rayleighScattering = rayleighScattering;
	model.mieDensity.width = 0.f;
	model.mieDensity.exponentialCoefficient = 1.f;
	model.mieDensity.exponentialScale = -1.f / 1.2f;
	model.mieDensity.heightScale = 0.f;
	model.mieDensity.offset = 0.f;
	model.mieScattering = { mieScattering, mieScattering, mieScattering };
	model.mieExtinction = { mieExtinction, mieExtinction, mieExtinction };
	model.absorptionDensity.width = 25.f;
	model.absorptionDensity.exponentialCoefficient = 0.f;
	model.absorptionDensity.exponentialScale = 0.f;
	model.absorptionDensity.heightScale = 1.f / 15.f;
	model.absorptionDensity.offset = -2.f / 3.f;
	model.absorptionExtinction = ozoneAbsorption;
	model.surfaceColor = { 0.1f, 0.1f, 0.1f };
	model.solarIrradiance = { 1.474f, 1.8504f, 1.91198f };

	sunLight = registry.create();
	registry.emplace<NameComponent>(sunLight, "Sun");
	registry.emplace<TransformComponent>(sunLight);
	// Note that directional light colors act as a multiplier against the sun, unlike other light types.
	registry.emplace<LightComponent>(sunLight, LightComponent{ .type = LightType::Directional, .color = { 1.f, 1.f, 1.f } });
	registry.emplace<TimeOfDayComponent>(sunLight, TimeOfDayComponent{ .solarZenithAngle = 0.f, .speed = 1.f, .animation = TimeOfDayAnimation::Oscillate });
}

AtmosphereResources Atmosphere::ImportResources(RenderGraph& graph)
{
	const auto transmittanceTag = graph.Import(transmittanceTexture);
	const auto scatteringTag = graph.Import(scatteringTexture);
	const auto irradianceTag = graph.Import(irradianceTexture);

	return { transmittanceTag, scatteringTag, irradianceTag };
}

void Atmosphere::Render(RenderGraph& graph, Clouds& clouds, AtmosphereResources resourceHandles, CloudResources cloudResources, RenderResource cameraBuffer,
	RenderResource depthStencil, RenderResource outputHDR, entt::registry& registry)
{
	if (dirty)
	{
		auto& precomputePass = graph.AddPass("Atmosphere Precompute Pass", ExecutionQueue::Compute);
		precomputePass.Write(resourceHandles.transmittanceHandle, ResourceBind::UAV);
		precomputePass.Write(resourceHandles.scatteringHandle, ResourceBind::UAV);
		precomputePass.Write(resourceHandles.irradianceHandle, ResourceBind::UAV);
		precomputePass.Bind([&, resourceHandles](CommandList& list, RenderPassResources& resources)
		{
			Precompute(list, resources.GetTexture(resourceHandles.transmittanceHandle), resources.GetTexture(resourceHandles.scatteringHandle), resources.GetTexture(resourceHandles.irradianceHandle));
		});
		
		dirty = false;
	}

	const auto solarZenithAngle = registry.get<TimeOfDayComponent>(sunLight).solarZenithAngle;

	// Update the sun light entity.
	auto& lightTransform = registry.get<TransformComponent>(sunLight);
	lightTransform.rotation = { 0.f, solarZenithAngle + 3.14159f / 2.f, 0.f };

	auto& composePass = graph.AddPass("Sky Atmosphere Compose Pass", ExecutionQueue::Compute);
	composePass.Read(cameraBuffer, ResourceBind::SRV);
	//composePass.Read(outputHDR, ResourceBind::SRV);
	composePass.Read(cloudResources.cloudsScatteringTransmittance, ResourceBind::SRV);
	composePass.Read(cloudResources.cloudsDepth, ResourceBind::SRV);
	composePass.Read(cloudResources.cloudsShadowMap, ResourceBind::SRV);
	composePass.Read(depthStencil, ResourceBind::SRV);
	composePass.Write(outputHDR, TextureView{}.UAV("", 0));

	composePass.Read(resourceHandles.transmittanceHandle, ResourceBind::SRV);
	composePass.Read(resourceHandles.scatteringHandle, ResourceBind::SRV);
	composePass.Read(resourceHandles.irradianceHandle, ResourceBind::SRV);

	composePass.Bind([&, cloudResources, cameraBuffer, depthStencil, outputHDR, resourceHandles, solarZenithAngle](CommandList& list, RenderPassResources& resources)
	{
		const auto renderShadowMap = *CvarGet("renderCloudShadowMap", int);
		const auto renderLightShafts = *CvarGet("renderLightShafts", int);

		auto composeLayout = RenderPipelineLayout{}
			.ComputeShader({ "Atmosphere/Compose", "Main" })
			.Macro({ "CLOUDS_RENDER_SHADOWMAP", renderShadowMap })
			.Macro({ "RENDER_LIGHT_SHAFTS", renderLightShafts });

		list.BindPipeline(composeLayout);

		struct {
			AtmosphereData atmosphere;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t cloudsScatteringTransmittanceTexture;
			uint32_t cloudsDepthTexture;
			uint32_t cloudsShadowMap;
			uint32_t geometryDepthTexture;
			uint32_t outputTexture;
			uint32_t transmissionTexture;
			uint32_t scatteringTexture;
			uint32_t irradianceTexture;
			float solarZenithAngle;
			float globalWeatherCoverage;
		} bindData;

		bindData.atmosphere = model;
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
		bindData.cloudsScatteringTransmittanceTexture = resources.Get(cloudResources.cloudsScatteringTransmittance);
		bindData.cloudsDepthTexture = resources.Get(cloudResources.cloudsDepth);
		bindData.cloudsShadowMap = resources.Get(cloudResources.cloudsShadowMap);
		bindData.geometryDepthTexture = resources.Get(depthStencil);
		bindData.outputTexture = resources.Get(outputHDR, "");
		bindData.transmissionTexture = resources.Get(resourceHandles.transmittanceHandle);
		bindData.scatteringTexture = resources.Get(resourceHandles.scatteringHandle);
		bindData.irradianceTexture = resources.Get(resourceHandles.irradianceHandle);
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.globalWeatherCoverage = clouds.coverage;

		list.BindConstants("bindData", bindData);

		const auto& outputComponent = device->GetResourceManager().Get(resources.GetTexture(outputHDR));
		const auto dispatchX = std::ceil(outputComponent.description.width / 8.f);
		const auto dispatchY = std::ceil(outputComponent.description.height / 8.f);

		list.Dispatch(dispatchX, dispatchY, 1);
	});
}

std::pair<RenderResource, RenderResource> Atmosphere::RenderEnvironmentMap(RenderGraph& graph, AtmosphereResources resourceHandles, RenderResource cameraBuffer,
	entt::registry& registry)
{
	const auto luminanceTag = graph.Import(luminanceTexture);

	const auto solarZenithAngle = registry.get<TimeOfDayComponent>(sunLight).solarZenithAngle;

	TextureView luminanceView{};
	luminanceView.UAV("", 0);

	auto& luminancePass = graph.AddPass("Atmosphere Luminance Pass", ExecutionQueue::Compute);
	luminancePass.Read(cameraBuffer, ResourceBind::SRV);
	luminancePass.Read(resourceHandles.transmittanceHandle, ResourceBind::SRV);
	luminancePass.Read(resourceHandles.scatteringHandle, ResourceBind::SRV);
	luminancePass.Read(resourceHandles.irradianceHandle, ResourceBind::SRV);
	luminancePass.Write(luminanceTag, luminanceView);
	luminancePass.Bind([&, cameraBuffer, resourceHandles, luminanceTag, solarZenithAngle](CommandList& list, RenderPassResources& resources)
	{
		struct BindData
		{
			AtmosphereData atmosphere;
			uint32_t transmissionTexture;
			uint32_t scatteringTexture;
			uint32_t irradianceTexture;
			float solarZenithAngle;
			uint32_t luminanceTexture;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
		} bindData;

		bindData.atmosphere = model;
		bindData.transmissionTexture = resources.Get(resourceHandles.transmittanceHandle);
		bindData.scatteringTexture = resources.Get(resourceHandles.scatteringHandle);
		bindData.irradianceTexture = resources.Get(resourceHandles.irradianceHandle);
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.luminanceTexture = resources.Get(luminanceTag);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.

		list.BindPipeline(luminancePrecomputeLayout);
		list.BindConstants("bindData", bindData);

		list.Dispatch(luminanceTextureSize / 8, luminanceTextureSize / 8, 6);

		list.UAVBarrier(luminanceTexture);
		list.FlushBarriers();

		device->GetResourceManager().GenerateMipmaps(list, luminanceTexture);
	});

	auto& irradiancePass = graph.AddPass("Atmosphere Separable Irradiance Pass", ExecutionQueue::Compute);
	irradiancePass.Read(cameraBuffer, ResourceBind::SRV);
	irradiancePass.Read(resourceHandles.transmittanceHandle, ResourceBind::SRV);
	irradiancePass.Read(resourceHandles.scatteringHandle, ResourceBind::SRV);
	irradiancePass.Read(resourceHandles.irradianceHandle, ResourceBind::SRV);
	const auto separableIrradiance = irradiancePass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // Unordered access.
		.size = 4,
		.stride = sizeof(XMFLOAT3)
	}, VGText("Atmosphere separable irradiance"));
	irradiancePass.Write(separableIrradiance, ResourceBind::UAV);
	irradiancePass.Bind([&, cameraBuffer, resourceHandles, separableIrradiance, solarZenithAngle](CommandList& list, RenderPassResources& resources)
	{
		struct BindData
		{
			AtmosphereData atmosphere;
			uint32_t transmissionTexture;
			uint32_t scatteringTexture;
			uint32_t irradianceTexture;
			float solarZenithAngle;
			uint32_t atmosphereIrradianceBuffer;
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
		} bindData;

		bindData.atmosphere = model;
		bindData.transmissionTexture = resources.Get(resourceHandles.transmittanceHandle);
		bindData.scatteringTexture = resources.Get(resourceHandles.scatteringHandle);
		bindData.irradianceTexture = resources.Get(resourceHandles.irradianceHandle);
		bindData.solarZenithAngle = solarZenithAngle;
		bindData.atmosphereIrradianceBuffer = resources.Get(separableIrradiance);
		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.

		list.BindPipeline(separableIrradianceLayout);
		list.BindConstants("bindData", bindData);

		list.Dispatch(1, 1, 1);
	});

	return std::make_pair(luminanceTag, separableIrradiance);
}