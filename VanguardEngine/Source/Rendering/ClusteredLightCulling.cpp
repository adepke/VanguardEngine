// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/ClusteredLightCulling.h>
#include <Rendering/Device.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderPass.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/ShaderStructs.h>
#include <Rendering/RenderComponents.h>
#include <Core/CoreComponents.h>

ClusterGridInfo ClusteredLightCulling::ComputeGridInfo(const entt::registry& registry) const
{
	// Make sure there's at least one camera.
	if (!registry.size<CameraComponent>())
	{
		return { 0, 0, 0, 0.f };
	}

	const auto& backBufferComponent = device->GetResourceManager().Get(device->GetBackBuffer());

	float cameraNearPlane;
	float cameraFarPlane;
	float cameraFOV;
	registry.view<const CameraComponent>().each([&](auto entity, const auto& camera)
	{
		cameraNearPlane = camera.nearPlane;
		cameraFarPlane = camera.farPlane;
		cameraFOV = camera.fieldOfView;
	});

	const auto x = static_cast<uint32_t>(std::ceil(backBufferComponent.description.width / (float)froxelSize));
	const auto y = static_cast<uint32_t>(std::ceil(backBufferComponent.description.height / (float)froxelSize));
	const float depthFactor = 1.f + (2.f * std::tan(cameraFOV / 2.f)) / (float)y;
	const auto z = static_cast<uint32_t>(std::floor(std::log(cameraFarPlane / cameraNearPlane) / std::log(depthFactor)));

	return { x, y, z, depthFactor };
}

void ClusteredLightCulling::ComputeClusterGrid(CommandList& list, const ClusterGridInfo& dimensions, BufferHandle cameraBuffer)
{
	constexpr auto groupSize = 8;

	const auto& backBufferComponent = device->GetResourceManager().Get(device->GetBackBuffer());

	struct ClusterData
	{
		int32_t gridDimensionsX;
		int32_t gridDimensionsY;
		int32_t gridDimensionsZ;
		float nearK;
		int32_t resolutionX;
		int32_t resolutionY;
		XMFLOAT2 padding;
	} clusterData;

	clusterData.gridDimensionsX = dimensions.x;
	clusterData.gridDimensionsY = dimensions.y;
	clusterData.gridDimensionsZ = dimensions.z;
	clusterData.nearK = dimensions.depthFactor;
	clusterData.resolutionX = backBufferComponent.description.width;
	clusterData.resolutionY = backBufferComponent.description.height;

	std::vector<uint32_t> clusterConstants;
	clusterConstants.resize(sizeof(ClusterData) / 4);
	std::memcpy(clusterConstants.data(), &clusterData, clusterConstants.size() * sizeof(uint32_t));

	uint32_t dispatchX = static_cast<uint32_t>(std::ceil(dimensions.x / (float)groupSize));
	uint32_t dispatchY = static_cast<uint32_t>(std::ceil(dimensions.y / (float)groupSize));

	// Compute tile frustums.

	list.BindPipelineState(viewFrustumsState);
	list.BindConstants("clusterData", clusterConstants);
	list.BindResource("clusterFrustums", clusterFrustums);
	list.BindResource("camera", cameraBuffer);
	list.Native()->Dispatch(dispatchX, dispatchY, 1);

	// Compute cluster AABBs.

	list.BindPipelineState(boundsState);
	list.BindConstants("clusterData", clusterConstants);
	list.BindResource("clusterAABBs", clusterAABBs);
	list.BindResource("camera", cameraBuffer);
	list.Native()->Dispatch(dispatchX, dispatchY, dimensions.z);

	list.UAVBarrier(clusterFrustums);
	list.UAVBarrier(clusterAABBs);
}

ClusteredLightCulling::~ClusteredLightCulling()
{
	device->GetResourceManager().Destroy(clusterFrustums);
	device->GetResourceManager().Destroy(clusterAABBs);
}

void ClusteredLightCulling::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	constexpr auto maxDivisionsX = 80;
	constexpr auto maxDivisionsY = 60;
	constexpr auto maxDivisionsZ = 160;

	BufferDescription clusterFrustumsDesc{};
	clusterFrustumsDesc.updateRate = ResourceFrequency::Static;
	clusterFrustumsDesc.bindFlags = BindFlag::UnorderedAccess;
	clusterFrustumsDesc.accessFlags = AccessFlag::GPUWrite;
	clusterFrustumsDesc.size = maxDivisionsX * maxDivisionsY;
	clusterFrustumsDesc.stride = 64;

	clusterFrustums = device->GetResourceManager().Create(clusterFrustumsDesc, VGText("Cluster frustums"));

	BufferDescription clusterAABBsDesc{};
	clusterAABBsDesc.updateRate = ResourceFrequency::Static;
	clusterAABBsDesc.bindFlags = BindFlag::UnorderedAccess | BindFlag::ShaderResource;
	clusterAABBsDesc.accessFlags = AccessFlag::GPUWrite;
	clusterAABBsDesc.size = maxDivisionsX * maxDivisionsY * maxDivisionsZ;
	clusterAABBsDesc.stride = 32;

	clusterAABBs = device->GetResourceManager().Create(clusterAABBsDesc, VGText("Cluster AABBs"));

	ComputePipelineStateDescription viewFrustumsStateDesc;
	viewFrustumsStateDesc.shader = { "ClusteredLightCulling_CS.hlsl", "ComputeClusterFrustumsMain" };
	viewFrustumsStateDesc.macros.emplace_back("FROXEL_SIZE", froxelSize);
	viewFrustumsState.Build(*device, viewFrustumsStateDesc);

	ComputePipelineStateDescription boundsStateDesc;
	boundsStateDesc.shader = { "ClusteredLightCulling_CS.hlsl", "ComputeClusterBoundsMain" };
	boundsStateDesc.macros.emplace_back("FROXEL_SIZE", froxelSize);
	boundsState.Build(*device, boundsStateDesc);

	GraphicsPipelineStateDescription depthCullStateDesc;
	depthCullStateDesc.vertexShader = { "ClusterDepthCulling.hlsl", "VSMain" };
	depthCullStateDesc.pixelShader = { "ClusterDepthCulling.hlsl", "PSMain" };
	depthCullStateDesc.macros.emplace_back("FROXEL_SIZE", froxelSize);
	depthCullStateDesc.blendDescription.AlphaToCoverageEnable = false;
	depthCullStateDesc.blendDescription.IndependentBlendEnable = false;
	depthCullStateDesc.blendDescription.RenderTarget[0].BlendEnable = false;
	depthCullStateDesc.blendDescription.RenderTarget[0].LogicOpEnable = false;
	depthCullStateDesc.blendDescription.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	depthCullStateDesc.blendDescription.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	depthCullStateDesc.blendDescription.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	depthCullStateDesc.blendDescription.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	depthCullStateDesc.blendDescription.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	depthCullStateDesc.blendDescription.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	depthCullStateDesc.blendDescription.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	depthCullStateDesc.blendDescription.RenderTarget[0].RenderTargetWriteMask = 0;
	depthCullStateDesc.rasterizerDescription.FillMode = D3D12_FILL_MODE_SOLID;
	depthCullStateDesc.rasterizerDescription.CullMode = D3D12_CULL_MODE_BACK;
	depthCullStateDesc.rasterizerDescription.FrontCounterClockwise = false;
	depthCullStateDesc.rasterizerDescription.DepthBias = 0;
	depthCullStateDesc.rasterizerDescription.DepthBiasClamp = 0.f;
	depthCullStateDesc.rasterizerDescription.SlopeScaledDepthBias = 0.f;
	depthCullStateDesc.rasterizerDescription.DepthClipEnable = true;
	depthCullStateDesc.rasterizerDescription.MultisampleEnable = false;
	depthCullStateDesc.rasterizerDescription.AntialiasedLineEnable = false;
	depthCullStateDesc.rasterizerDescription.ForcedSampleCount = 0;
	depthCullStateDesc.rasterizerDescription.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	depthCullStateDesc.depthStencilDescription.DepthEnable = true;
	depthCullStateDesc.depthStencilDescription.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthCullStateDesc.depthStencilDescription.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;  // Transparents receive lighting, so they cannot be culled.
	depthCullStateDesc.depthStencilDescription.StencilEnable = false;
	depthCullStateDesc.depthStencilDescription.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthCullStateDesc.depthStencilDescription.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	depthCullStateDesc.depthStencilDescription.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthCullStateDesc.depthStencilDescription.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	depthCullState.Build(*device, depthCullStateDesc, false);
}

void ClusteredLightCulling::Render(RenderGraph& graph, const entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil, BufferHandle instanceBuffer, size_t instanceOffset)
{
	ClusterGridInfo dimensions = ComputeGridInfo(registry);
	if (dimensions.x == 0 || dimensions.y == 0 || dimensions.z == 0)
	{
		return;
	}

	if (dirty)
	{
		auto clusterFrustumsTag = graph.Import(clusterFrustums);
		auto clusterAABBsTag = graph.Import(clusterAABBs);

		auto& computeClusterGridPass = graph.AddPass("Compute Cluster Grid Pass", ExecutionQueue::Compute);
		computeClusterGridPass.Read(cameraBuffer, ResourceBind::CBV);
		computeClusterGridPass.Write(clusterFrustumsTag, ResourceBind::UAV);
		computeClusterGridPass.Write(clusterAABBsTag, ResourceBind::UAV);
		computeClusterGridPass.Bind([&, dimensions, cameraBuffer](CommandList& list, RenderGraphResourceManager& resources)
		{
			ComputeClusterGrid(list, dimensions, resources.GetBuffer(cameraBuffer));
		});

		dirty = false;
	}

	auto& clusterDepthCullingPass = graph.AddPass("Cluster Depth Culling", ExecutionQueue::Graphics);
	clusterDepthCullingPass.Read(cameraBuffer, ResourceBind::CBV);
	clusterDepthCullingPass.Read(depthStencil, ResourceBind::DSV);
	const auto clusterVisibilityTag = clusterDepthCullingPass.Create(TransientBufferDescription{
		.updateRate = ResourceFrequency::Static,  // Must be static for UAVs.
		.size = dimensions.x * dimensions.y * dimensions.z,
		.stride = sizeof(bool) * 4  // Structured buffers pad each element out to 4 bytes.
	}, VGText("Cluster visibility"));
	clusterDepthCullingPass.Write(clusterVisibilityTag, ResourceBind::UAV);
	clusterDepthCullingPass.Bind([&, dimensions, cameraBuffer, clusterVisibilityTag, instanceBuffer, instanceOffset](CommandList& list, RenderGraphResourceManager& resources)
	{
		// #TODO: Use ClearUnorderedAccessView when Cluster Visibility becomes a normal buffer, instead of a structured buffer.

		list.BindPipelineState(depthCullState);
		list.BindResource("camera", resources.GetBuffer(cameraBuffer));
		list.BindResource("clusterVisibility", resources.GetBuffer(clusterVisibilityTag));

		struct ClusterData
		{
			int32_t gridDimensionsX;
			int32_t gridDimensionsY;
			int32_t gridDimensionsZ;
			float logY;
		} clusterData;

		clusterData.gridDimensionsX = dimensions.x;
		clusterData.gridDimensionsY = dimensions.y;
		clusterData.gridDimensionsZ = dimensions.z;
		clusterData.logY = 1.f / std::log(dimensions.depthFactor);

		std::vector<uint32_t> clusterConstants;
		clusterConstants.resize(sizeof(ClusterData) / 4);
		std::memcpy(clusterConstants.data(), &clusterData, clusterConstants.size() * sizeof(uint32_t));

		list.BindConstants("clusterData", clusterConstants);

		size_t entityIndex = 0;
		registry.view<const TransformComponent, const MeshComponent>().each([&](auto entity, const auto&, const auto& mesh)
		{
			list.BindResource("perObject", instanceBuffer, instanceOffset + (entityIndex * sizeof(EntityInstance)));

			// Set the index buffer.
			auto& indexBuffer = device->GetResourceManager().Get(mesh.indexBuffer);
			D3D12_INDEX_BUFFER_VIEW indexView{};
			indexView.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress();
			indexView.SizeInBytes = static_cast<UINT>(indexBuffer.description.size * indexBuffer.description.stride);
			indexView.Format = DXGI_FORMAT_R32_UINT;

			list.Native()->IASetIndexBuffer(&indexView);

			for (const auto& subset : mesh.subsets)
			{
				// #TODO: Only bind once per mesh, and pass subset.vertexOffset into the draw call. This isn't yet supported with DXC, see: https://github.com/microsoft/DirectXShaderCompiler/issues/2907
				list.BindResource("vertexBuffer", mesh.vertexBuffer, subset.vertexOffset * sizeof(Vertex));

				list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, static_cast<uint32_t>(subset.indexOffset), 0, 0);
			}

			++entityIndex;
		});
	});
}