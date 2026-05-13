// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/DebugDraw.h>
#include <Rendering/Device.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderPipeline.h>
#include <Rendering/Resource.h>
#include <Rendering/ClusteredLightCulling.h>

#include <array>
#include <utility>

namespace
{
	// Some of these have matching values in the shader, keep in sync.
	// #TODO: Pull these into shader defines so they can't desync.
	constexpr uint32_t cubeVertexCount = 24;  // 12 edges * 2 endpoints.
	constexpr uint32_t sphereSegments = 32;
	constexpr uint32_t sphereGreatCircles = 3;
	constexpr uint32_t sphereVertexCount = sphereGreatCircles * sphereSegments * 2;
}

void DebugDraw::Initialize(RenderDevice* inDevice)
{
	device = inDevice;
}

void DebugDraw::DrawCube(const XMMATRIX& transform, const XMFLOAT4& color, bool depthTest)
{
	cubes[depthTest ? DepthTested : AlwaysOnTop].emplace_back(DebugCubeInstance{ transform, color });
}

void DebugDraw::DrawSphere(const XMFLOAT3& center, float radius, const XMFLOAT4& color, bool depthTest)
{
	spheres[depthTest ? DepthTested : AlwaysOnTop].emplace_back(DebugSphereInstance{ center, radius, color });
}

void DebugDraw::Render(RenderGraph& graph, RenderResource cameraBuffer, RenderResource depthStencil, RenderResource outputTarget)
{
	// Snapshot the queue state and clear immediately so user-side draw calls during the next
	// frame don't accumulate twice. The lambda below captures by value.
	std::array<std::vector<DebugCubeInstance>, ModeCount> queuedCubes;
	std::array<std::vector<DebugSphereInstance>, ModeCount> queuedSpheres;
	for (int i = 0; i < ModeCount; ++i)
	{
		queuedCubes[i] = std::move(cubes[i]);
		cubes[i].clear();
		queuedSpheres[i] = std::move(spheres[i]);
		spheres[i].clear();
	}

	// Early exit if nothing to draw.
	bool anyShapes = false;
	for (int i = 0; i < ModeCount; ++i)
	{
		if (!queuedCubes[i].empty() || !queuedSpheres[i].empty())
		{
			anyShapes = true;
			break;
		}
	}
	if (!anyShapes)
	{
		return;
	}

	// Upload all shape data into GPU buffers.
	std::array<BufferHandle, ModeCount> cubeBuffers{};
	std::array<BufferHandle, ModeCount> sphereBuffers{};
	for (int i = 0; i < ModeCount; ++i)
	{
		if (!queuedCubes[i].empty())
		{
			BufferDescription desc{};
			desc.updateRate = ResourceFrequency::Dynamic;
			desc.bindFlags = BindFlag::ShaderResource;
			desc.accessFlags = AccessFlag::CPUWrite;
			desc.size = queuedCubes[i].size();
			desc.stride = sizeof(DebugCubeInstance);
			cubeBuffers[i] = device->GetResourceManager().Create(desc, VGText("Debug draw cube buffer"));
			device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), cubeBuffers[i]);
			device->GetResourceManager().Write(cubeBuffers[i], queuedCubes[i]);
		}
		if (!queuedSpheres[i].empty())
		{
			BufferDescription desc{};
			desc.updateRate = ResourceFrequency::Dynamic;
			desc.bindFlags = BindFlag::ShaderResource;
			desc.accessFlags = AccessFlag::CPUWrite;
			desc.size = queuedSpheres[i].size();
			desc.stride = sizeof(DebugSphereInstance);
			sphereBuffers[i] = device->GetResourceManager().Create(desc, VGText("Debug draw sphere buffer"));
			device->GetResourceManager().AddFrameResource(device->GetFrameIndex(), sphereBuffers[i]);
			device->GetResourceManager().Write(sphereBuffers[i], queuedSpheres[i]);
		}
	}

	auto& pass = graph.AddPass("Debug Draw Pass", ExecutionQueue::Graphics);
	pass.Read(cameraBuffer, ResourceBind::SRV);
	pass.Read(depthStencil, ResourceBind::DSV);

	std::array<RenderResource, ModeCount> cubeBufferTags{};
	std::array<RenderResource, ModeCount> sphereBufferTags{};
	for (int i = 0; i < ModeCount; ++i)
	{
		if (cubeBuffers[i].handle != entt::null)
		{
			cubeBufferTags[i] = graph.Import(cubeBuffers[i]);
			pass.Read(cubeBufferTags[i], ResourceBind::SRV);
		}
		if (sphereBuffers[i].handle != entt::null)
		{
			sphereBufferTags[i] = graph.Import(sphereBuffers[i]);
			pass.Read(sphereBufferTags[i], ResourceBind::SRV);
		}
	}

	pass.Output(outputTarget, OutputBind::RTV, LoadType::Preserve);
	pass.Bind([cameraBuffer, cubeBufferTags, sphereBufferTags,
		queuedCubes = std::move(queuedCubes), queuedSpheres = std::move(queuedSpheres)]
		(CommandList& list, RenderPassResources& resources)
	{
		auto MakeLayout = [](const char* vertexShader, DepthTestFunction depthFunc)
		{
			return RenderPipelineLayout{}
				.VertexShader({ "DebugDraw", vertexShader })
				.PixelShader({ "DebugDraw", "PSMain" })
				.Topology(D3D_PRIMITIVE_TOPOLOGY_LINELIST)
				.CullMode(D3D12_CULL_MODE_NONE)
				.DepthEnabled(true, false, depthFunc);
		};

		struct
		{
			uint32_t cameraBuffer;
			uint32_t cameraIndex;
			uint32_t shapeBuffer;
			float padding;
		} bindData;

		bindData.cameraBuffer = resources.Get(cameraBuffer);
		bindData.cameraIndex = 0;  // #TODO: Multiple camera support.

		for (int mode = 0; mode < ModeCount; ++mode)
		{
			const auto depthFunc = (mode == DepthTested) ? DepthTestFunction::Greater : DepthTestFunction::Always;

			if (!queuedCubes[mode].empty())
			{
				list.BindPipeline(MakeLayout("VSCube", depthFunc));

				bindData.shapeBuffer = resources.Get(cubeBufferTags[mode]);
				list.BindConstants("bindData", bindData);

				const auto count = static_cast<uint32_t>(queuedCubes[mode].size());
				list.DrawInstanced(cubeVertexCount, count, 0, 0);
			}

			if (!queuedSpheres[mode].empty())
			{
				list.BindPipeline(MakeLayout("VSSphere", depthFunc));

				bindData.shapeBuffer = resources.Get(sphereBufferTags[mode]);
				list.BindConstants("bindData", bindData);

				const auto count = static_cast<uint32_t>(queuedSpheres[mode].size());
				list.DrawInstanced(sphereVertexCount, count, 0, 0);
			}
		}
	});
}
