// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/Atmosphere.h>
#include <Rendering/Device.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/ResourceManager.h>
#include <Rendering/PipelineBuilder.h>

#include <vector>
#include <cmath>

void Atmosphere::Precompute(CommandList& list)
{
	constexpr auto groupSize = 8;
	constexpr auto scatteringOrder = 4;

	const auto& transmittanceComponent = device->GetResourceManager().Get(transmittanceTexture);
	const auto& scatteringComponent = device->GetResourceManager().Get(scatteringTexture);
	const auto& irradianceComponent = device->GetResourceManager().Get(irradianceTexture);
	const auto& deltaRayleighComponent = device->GetResourceManager().Get(deltaRayleighTexture);
	const auto& deltaMieComponent = device->GetResourceManager().Get(deltaMieTexture);
	const auto& deltaScatteringDensityComponent = device->GetResourceManager().Get(deltaScatteringDensityTexture);
	const auto& deltaIrradianceComponent = device->GetResourceManager().Get(deltaIrradianceTexture);

	struct PrecomputeData
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
	} precomputeData;

	precomputeData.atmosphere = model;

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

	precomputeData.transmissionTexture = transmissionUAV.bindlessIndex;
	precomputeData.scatteringTexture = 0;
	precomputeData.irradianceTexture = 0;
	precomputeData.deltaRayleighTexture = 0;
	precomputeData.deltaMieTexture = 0;
	precomputeData.deltaScatteringDensityTexture = 0;
	precomputeData.deltaIrradianceTexture = 0;

	list.TransitionBarrier(transmittanceTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.FlushBarriers();

	list.BindPipelineState(transmissionPrecompute);
	list.BindDescriptorAllocator(device->GetDescriptorAllocator());
	list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());
	list.BindConstants("precomputeData", precomputeData);
	list.Native()->Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
	list.UAVBarrier(transmittanceTexture);

	// Direct irradiance.

	dispatchX = std::ceil((float)irradianceComponent.description.width / groupSize);
	dispatchY = std::ceil((float)irradianceComponent.description.height / groupSize);
	dispatchZ = 1.f;

	precomputeData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
	precomputeData.scatteringTexture = 0;
	precomputeData.irradianceTexture = irradianceUAV.bindlessIndex;
	precomputeData.deltaRayleighTexture = 0;
	precomputeData.deltaMieTexture = 0;
	precomputeData.deltaScatteringDensityTexture = 0;
	precomputeData.deltaIrradianceTexture = deltaIrradianceUAV.bindlessIndex;

	list.TransitionBarrier(transmittanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list.TransitionBarrier(irradianceTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.TransitionBarrier(deltaIrradianceTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.FlushBarriers();

	list.BindPipelineState(directIrradiancePrecompute);
	list.BindDescriptorAllocator(device->GetDescriptorAllocator());
	list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
	list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());
	list.BindConstants("precomputeData", precomputeData);
	list.Native()->Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
	list.UAVBarrier(deltaIrradianceTexture);
	list.UAVBarrier(irradianceTexture);

	// Single scattering.

	dispatchX = std::ceil((float)scatteringComponent.description.width / groupSize);
	dispatchY = std::ceil((float)scatteringComponent.description.height / groupSize);
	dispatchZ = std::ceil((float)scatteringComponent.description.depth / 1.f);

	precomputeData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
	precomputeData.scatteringTexture = scatteringUAV.bindlessIndex;
	precomputeData.irradianceTexture = 0;
	precomputeData.deltaRayleighTexture = deltaRayleighUAV.bindlessIndex;
	precomputeData.deltaMieTexture = deltaMieUAV.bindlessIndex;
	precomputeData.deltaScatteringDensityTexture = 0;
	precomputeData.deltaIrradianceTexture = 0;

	list.TransitionBarrier(transmittanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list.TransitionBarrier(scatteringTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.TransitionBarrier(deltaRayleighTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.TransitionBarrier(deltaMieTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	list.FlushBarriers();

	list.BindPipelineState(singleScatteringPrecompute);
	list.BindDescriptorAllocator(device->GetDescriptorAllocator());
	list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
	list.BindResourceTable("textures3DRW", device->GetDescriptorAllocator().GetBindlessHeap());
	list.BindConstants("precomputeData", precomputeData);
	list.Native()->Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
	list.UAVBarrier(deltaRayleighTexture);
	list.UAVBarrier(deltaMieTexture);
	list.UAVBarrier(scatteringTexture);

	for (int i = 2; i <= scatteringOrder; ++i)
	{
		std::string zoneName = "Precompute scattering order " + std::to_string(i);
		VGScopedGPUTransientStat(zoneName.c_str(), device->GetDirectContext(), list.Native());

		// Scattering density.

		dispatchX = std::ceil((float)deltaScatteringDensityComponent.description.width / groupSize);
		dispatchY = std::ceil((float)deltaScatteringDensityComponent.description.height / groupSize);
		dispatchZ = std::ceil((float)deltaScatteringDensityComponent.description.depth / 1.f);

		precomputeData.scatteringOrder = i;
		precomputeData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
		precomputeData.scatteringTexture = 0;
		precomputeData.irradianceTexture = 0;
		precomputeData.deltaRayleighTexture = deltaRayleighComponent.SRV->bindlessIndex;
		precomputeData.deltaMieTexture = deltaMieComponent.SRV->bindlessIndex;
		precomputeData.deltaScatteringDensityTexture = deltaScatteringDensityUAV.bindlessIndex;
		precomputeData.deltaIrradianceTexture = deltaIrradianceComponent.SRV->bindlessIndex;

		list.TransitionBarrier(transmittanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaRayleighTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaMieTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaScatteringDensityTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.TransitionBarrier(deltaIrradianceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.FlushBarriers();

		list.BindPipelineState(scatteringDensityPrecompute);
		list.BindDescriptorAllocator(device->GetDescriptorAllocator());
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("textures3D", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("textures3DRW", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindConstants("precomputeData", precomputeData);
		list.Native()->Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
		list.UAVBarrier(deltaScatteringDensityTexture);

		// Indirect irradiance.

		dispatchX = std::ceil((float)irradianceComponent.description.width / groupSize);
		dispatchY = std::ceil((float)irradianceComponent.description.height / groupSize);
		dispatchZ = 1.f;

		precomputeData.scatteringOrder = i - 1;
		precomputeData.transmissionTexture = 0;
		precomputeData.scatteringTexture = 0;
		precomputeData.irradianceTexture = irradianceUAV.bindlessIndex;
		precomputeData.deltaRayleighTexture = deltaRayleighComponent.SRV->bindlessIndex;
		precomputeData.deltaMieTexture = deltaMieComponent.SRV->bindlessIndex;
		precomputeData.deltaScatteringDensityTexture = 0;
		precomputeData.deltaIrradianceTexture = deltaIrradianceUAV.bindlessIndex;

		list.TransitionBarrier(irradianceTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.TransitionBarrier(deltaRayleighTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaMieTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(deltaIrradianceTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.FlushBarriers();

		list.BindPipelineState(indirectIrradiancePrecompute);
		list.BindDescriptorAllocator(device->GetDescriptorAllocator());
		list.BindResourceTable("textures3D", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindConstants("precomputeData", precomputeData);
		list.Native()->Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
		list.UAVBarrier(deltaIrradianceTexture);
		list.UAVBarrier(irradianceTexture);

		// Multiple scattering.

		dispatchX = std::ceil((float)scatteringComponent.description.width / groupSize);
		dispatchY = std::ceil((float)scatteringComponent.description.height / groupSize);
		dispatchZ = std::ceil((float)scatteringComponent.description.depth / 1.f);

		precomputeData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
		precomputeData.scatteringTexture = scatteringUAV.bindlessIndex;
		precomputeData.irradianceTexture = 0;
		precomputeData.deltaRayleighTexture = deltaRayleighUAV.bindlessIndex;
		precomputeData.deltaMieTexture = 0;
		precomputeData.deltaScatteringDensityTexture = deltaScatteringDensityComponent.SRV->bindlessIndex;
		precomputeData.deltaIrradianceTexture = 0;

		list.TransitionBarrier(transmittanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(scatteringTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.TransitionBarrier(deltaRayleighTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.TransitionBarrier(deltaScatteringDensityTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list.FlushBarriers();

		list.BindPipelineState(multipleScatteringPrecompute);
		list.BindDescriptorAllocator(device->GetDescriptorAllocator());
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("textures3DRW", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindConstants("precomputeData", precomputeData);
		list.Native()->Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
		list.UAVBarrier(deltaRayleighTexture);
		list.UAVBarrier(scatteringTexture);
	}

	list.TransitionBarrier(transmittanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list.TransitionBarrier(scatteringTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list.TransitionBarrier(irradianceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list.FlushBarriers();

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
}

void Atmosphere::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	ComputePipelineStateDescription precomputeState;
	precomputeState.shader = { "AtmospherePrecompute_CS", "TransmittanceLutMain" };
	transmissionPrecompute.Build(*device, precomputeState);

	precomputeState.shader = { "AtmospherePrecompute_CS", "DirectIrradianceLutMain" };
	directIrradiancePrecompute.Build(*device, precomputeState);

	precomputeState.shader = { "AtmospherePrecompute_CS", "SingleScatteringLutMain" };
	singleScatteringPrecompute.Build(*device, precomputeState);

	precomputeState.shader = { "AtmospherePrecompute_CS", "ScatteringDensityLutMain" };
	scatteringDensityPrecompute.Build(*device, precomputeState);

	precomputeState.shader = { "AtmospherePrecompute_CS", "IndirectIrradianceLutMain" };
	indirectIrradiancePrecompute.Build(*device, precomputeState);

	precomputeState.shader = { "AtmospherePrecompute_CS", "MultipleScatteringLutMain" };
	multipleScatteringPrecompute.Build(*device, precomputeState);

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
}

void Atmosphere::Render(RenderGraph& graph, PipelineBuilder& pipelines, RenderResource cameraBuffer, RenderResource depthStencil, RenderResource outputHDR)
{
	if (dirty)
	{
		auto& precomputePass = graph.AddPass("Atmosphere Precompute Pass", ExecutionQueue::Compute);
		precomputePass.Bind([&](CommandList& list, RenderGraphResourceManager& resources)
		{
			Precompute(list);
		});
		
		dirty = false;
	}

	auto& atmospherePass = graph.AddPass("Atmosphere Pass", ExecutionQueue::Graphics);
	atmospherePass.Read(cameraBuffer, ResourceBind::CBV);
	atmospherePass.Read(depthStencil, ResourceBind::DSV);
	atmospherePass.Output(outputHDR, OutputBind::RTV, false);
	atmospherePass.Bind([&, cameraBuffer, depthStencil, outputHDR](CommandList& list, RenderGraphResourceManager& resources)
	{
		struct BindData
		{
			AtmosphereData atmosphere;
			uint32_t transmissionTexture;
			uint32_t scatteringTexture;
			uint32_t irradianceTexture;
			float solarZenithAngle;
		} bindData;
		
		bindData.atmosphere = model;
		bindData.transmissionTexture = device->GetResourceManager().Get(transmittanceTexture).SRV->bindlessIndex;
		bindData.scatteringTexture = device->GetResourceManager().Get(scatteringTexture).SRV->bindlessIndex;
		bindData.irradianceTexture = device->GetResourceManager().Get(irradianceTexture).SRV->bindlessIndex;
		bindData.solarZenithAngle = solarZenithAngle;

		list.BindPipelineState(pipelines["Atmosphere"]);
		list.BindResource("camera", resources.GetBuffer(cameraBuffer));
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("textures3D", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindConstants("bindData", bindData);

		list.DrawFullscreenQuad();
	});
}