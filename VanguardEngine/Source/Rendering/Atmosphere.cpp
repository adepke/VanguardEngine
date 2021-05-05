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

	const auto& transmittanceComponent = device->GetResourceManager().Get(transmittanceTexture);
	const auto& scatteringComponent = device->GetResourceManager().Get(scatteringTexture);
	const auto& irradianceComponent = device->GetResourceManager().Get(irradianceTexture);

	struct PrecomputeData
	{
		AtmosphereData atmosphere;
		uint32_t transmissionTexture;
		uint32_t scatteringTexture;
		uint32_t irradianceTexture;
		float padding;
	} precomputeData;

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

	precomputeData.atmosphere = model;
	precomputeData.transmissionTexture = transmissionUAV.bindlessIndex;
	precomputeData.scatteringTexture = scatteringUAV.bindlessIndex;
	precomputeData.irradianceTexture = irradianceComponent.SRV->bindlessIndex;

	std::vector<uint32_t> constantBufferData;
	constantBufferData.resize(sizeof(PrecomputeData) / 4);
	std::memcpy(constantBufferData.data(), &precomputeData, constantBufferData.size() * sizeof(uint32_t));

	// Transmission.

	auto dispatchX = std::ceil((float)transmittanceComponent.description.width / groupSize);
	auto dispatchY = std::ceil((float)transmittanceComponent.description.height / groupSize);
	auto dispatchZ = 1.f;

	list.BindPipelineState(transmissionPrecompute);
	list.BindDescriptorAllocator(device->GetDescriptorAllocator());
	list.BindResourceTable("texturesRW", device->GetDescriptorAllocator().GetBindlessHeap());
	list.BindConstants("precomputeData", constantBufferData);
	list.Native()->Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
	list.UAVBarrier(transmittanceTexture);

	list.TransitionBarrier(transmittanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	list.FlushBarriers();

	// Scattering.

	dispatchX = std::ceil((float)scatteringComponent.description.width / groupSize);
	dispatchY = std::ceil((float)scatteringComponent.description.height / groupSize);
	dispatchZ = std::ceil((float)scatteringComponent.description.depth / 1.f);

	// Now a read-only transmission texture.
	precomputeData.transmissionTexture = transmittanceComponent.SRV->bindlessIndex;
	std::memcpy(constantBufferData.data(), &precomputeData, constantBufferData.size() * sizeof(uint32_t));

	list.BindPipelineState(scatteringPrecompute);
	list.BindDescriptorAllocator(device->GetDescriptorAllocator());
	list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
	list.BindResourceTable("textures3DRW", device->GetDescriptorAllocator().GetBindlessHeap());
	list.BindConstants("precomputeData", constantBufferData);
	list.Native()->Dispatch((uint32_t)dispatchX, (uint32_t)dispatchY, (uint32_t)dispatchZ);
	list.UAVBarrier(scatteringTexture);

	list.TransitionBarrier(scatteringTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	list.FlushBarriers();

	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(transmissionUAV));
	device->GetResourceManager().AddFrameDescriptor(device->GetFrameIndex(), std::move(scatteringUAV));
}

Atmosphere::~Atmosphere()
{
	device->GetResourceManager().Destroy(transmittanceTexture);
	device->GetResourceManager().Destroy(scatteringTexture);
	device->GetResourceManager().Destroy(irradianceTexture);
}

void Atmosphere::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	ComputePipelineStateDescription precomputeState;
	precomputeState.shader = { "AtmospherePrecompute_CS", "TransmittanceLutMain" };
	transmissionPrecompute.Build(*device, precomputeState);

	precomputeState.shader = { "AtmospherePrecompute_CS", "ScatteringLutMain" };
	scatteringPrecompute.Build(*device, precomputeState);

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

	// Meters.
	XMFLOAT3 rayleighScattering = { 0.0000058f, 0.0000135f, 0.0000331f };  // Frostbite's.
	float mieScattering = 0.000004f;
	float mieExtinction = 1.11f * mieScattering;  // Frostbite's.
	XMFLOAT3 ozoneAbsorption = { 0.0000020556f, 0.0000049788f, 0.0000002136f };  // Frostbite's.

	model.radius = 6360000.f;
	model.rayleighDensity.width = 60000.f;
	model.rayleighDensity.exponentialCoefficient = 1.f;
	model.rayleighDensity.exponentialScale = -1.f / 8000.f;
	model.rayleighDensity.heightScale = 0.f;
	model.rayleighDensity.offset = 0.f;
	model.rayleighScattering = rayleighScattering;
	model.mieDensity.width = 20000.f;
	model.mieDensity.exponentialCoefficient = 1.f;
	model.mieDensity.exponentialScale = -1.f / 1200.f;
	model.mieDensity.heightScale = 0.f;
	model.mieDensity.offset = 0.f;
	model.mieScattering = { mieScattering, mieScattering, mieScattering };
	model.mieExtinction = { mieExtinction, mieExtinction, mieExtinction };
	model.absorptionDensity.width = 25000.f;
	model.absorptionDensity.exponentialCoefficient = 0.f;
	model.absorptionDensity.exponentialScale = 0.f;
	model.absorptionDensity.heightScale = 1.f / 15000.f;
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
			XMFLOAT2 padding;
		} bindData;
		
		bindData.atmosphere = model;
		bindData.transmissionTexture = device->GetResourceManager().Get(transmittanceTexture).SRV->bindlessIndex;
		bindData.scatteringTexture = device->GetResourceManager().Get(scatteringTexture).SRV->bindlessIndex;

		std::vector<uint32_t> bindConstants;
		bindConstants.resize(sizeof(BindData) / 4);
		std::memcpy(bindConstants.data(), &bindData, bindConstants.size() * sizeof(uint32_t));

		list.BindPipelineState(pipelines["Atmosphere"]);
		list.BindResource("camera", resources.GetBuffer(cameraBuffer));
		list.BindResourceTable("textures", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindResourceTable("textures3D", device->GetDescriptorAllocator().GetBindlessHeap());
		list.BindConstants("bindData", bindConstants);

		list.DrawFullscreenQuad();
	});
}