// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/AccelerationStructures.h>
#include <Rendering/CommandList.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/MeshFactory.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/Object.h>
#include <Core/CoreComponents.h>
#include <Core/ConsoleVariable.h>
#include <Utility/AlignedSize.h>

#include <memory>
#include <vector>
#include <algorithm>

// #TODO: refactor a bit.
void AccelerationStructures::EnsureScratch(size_t bytes)
{
	if (bytes <= scratchSize)
	{
		return;
	}

	// Need to grow, discard the current buffer and get a new one.
	auto& resourceManager = device->GetResourceManager();

	if (resourceManager.Valid(scratch))
	{
		resourceManager.AddFrameResource(device->GetFrameIndex(), scratch);
	}

	BufferDescription description{
		.updateRate = ResourceFrequency::Static,
		.bindFlags = BindFlag::UnorderedAccess,
		.accessFlags = AccessFlag::GPUWrite,
		.size = bytes,
		.stride = 1
	};

	scratch = resourceManager.Create(description, VGText("Acceleration structure scratch"));
	scratchSize = bytes;
}

void AccelerationStructures::EnsureInstanceBuffer(size_t count)
{
	auto& resourceManager = device->GetResourceManager();

	// All frame buffers are created together, so checking the first is sufficient.
	if (count <= instanceCapacity && resourceManager.Valid(instanceBuffers[0]))
	{
		return;
	}

	// Grow with slack to avoid recreating every time an instance is added.
	instanceCapacity = std::max<size_t>(count + count / 2, 128);

	for (auto& buffer : instanceBuffers)
	{
		if (resourceManager.Valid(buffer))
		{
			resourceManager.AddFrameResource(device->GetFrameIndex(), buffer);
		}

		BufferDescription description{
			.updateRate = ResourceFrequency::Dynamic,
			.bindFlags = 0,
			.accessFlags = AccessFlag::CPUWrite,
			.size = instanceCapacity,
			.stride = sizeof(D3D12_RAYTRACING_INSTANCE_DESC)
		};

		buffer = resourceManager.Create(description, VGText("TLAS instance buffer"));
	}
}

void AccelerationStructures::EnsureTlas(size_t bytes)
{
	if (bytes <= tlasSize)
	{
		return;
	}

	auto& resourceManager = device->GetResourceManager();

	if (resourceManager.Valid(tlas))
	{
		resourceManager.AddFrameResource(device->GetFrameIndex(), tlas);
	}

	BufferDescription description{
		.updateRate = ResourceFrequency::Static,
		.bindFlags = BindFlag::AccelerationStructure,
		.accessFlags = AccessFlag::GPUWrite,
		.size = bytes,
		.stride = 1
	};

	tlas = resourceManager.Create(description, VGText("Top level acceleration structure"));
	tlasSize = bytes;
}

AccelerationStructures::~AccelerationStructures()
{
	if (!device)
	{
		return;
	}

	auto& resourceManager = device->GetResourceManager();

	for (auto& [key, blas] : blasCache)
	{
		if (resourceManager.Valid(blas))
		{
			resourceManager.Destroy(blas);
		}
	}

	if (resourceManager.Valid(tlas)) resourceManager.Destroy(tlas);
	if (resourceManager.Valid(scratch)) resourceManager.Destroy(scratch);
	for (auto& buffer : instanceBuffers)
	{
		if (resourceManager.Valid(buffer)) resourceManager.Destroy(buffer);
	}
}

void AccelerationStructures::Initialize(RenderDevice* inDevice)
{
	device = inDevice;

	CvarCreate("rayTracingEnabled", "Gate for all ray tracing features. 0=off, 1=on", device->supportsRayTracing ? 1 : 0);
}

AccelerationStructureResources AccelerationStructures::Render(RenderGraph& graph, entt::registry& registry, MeshFactory& meshFactory, RenderResource meshPositionTag)
{
	VGScopedCPUStat("Acceleration Structures");

	if (CvarGet("rayTracingEnabled", int) == 0)
	{
		return {};
	}

	auto& resourceManager = device->GetResourceManager();

	const auto positionAddress = resourceManager.Get(meshFactory.vertexPositionBuffer).Native()->GetGPUVirtualAddress();
	const auto indexAddress = resourceManager.Get(meshFactory.indexBuffer).Native()->GetGPUVirtualAddress();

	// #TODO: BLAS builds are one-shot in a single frame for simplicity: a scene load with many new
	// meshes will hitch on the frame that builds them all. The better solution is to run acceleration
	// structure builds on an async compute queue and amortize them across frames. Renderer needs to
	// support async compute first.

	struct BlasBuild
	{
		BufferHandle blas;
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry;
		size_t scratchOffset = 0;
	};

	auto builds = std::make_shared<std::vector<BlasBuild>>();
	size_t scratchBytes = 0;

	// First pass: create BLASes for meshes we haven't seen before.
	auto meshView = registry.view<const TransformComponent, const MeshComponent>();
	meshView.each([&](entt::entity entity, const TransformComponent& transform, const MeshComponent& mesh)
	{
		const size_t key = mesh.globalOffset.position;
		if (blasCache.contains(key))
		{
			return;
		}

		BlasBuild build;
		build.geometry.reserve(mesh.subsets.size());

		// Note that index data is subset-local (the draw path applies vertex offsets through the
		// object's vertex metadata), so each subset gets its own geometry description with base
		// addresses offset into the mesh factory's shared buffers.
		for (const auto& subset : mesh.subsets)
		{
			D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
			geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;  // #TODO: Proper alpha-testing support.
			geometry.Triangles.Transform3x4 = 0;
			geometry.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
			geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			geometry.Triangles.IndexCount = static_cast<UINT>(subset.indices);
			geometry.Triangles.VertexCount = static_cast<UINT>(subset.vertices);
			geometry.Triangles.IndexBuffer = indexAddress + mesh.globalOffset.index + subset.localOffset.index;
			geometry.Triangles.VertexBuffer.StartAddress = positionAddress + mesh.globalOffset.position + subset.localOffset.position;
			geometry.Triangles.VertexBuffer.StrideInBytes = mesh.metadata.channelStrides[0].values[0];  // Position channel stride.

			build.geometry.emplace_back(geometry);
		}

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;  // Static geometry, built once.
		inputs.NumDescs = static_cast<UINT>(build.geometry.size());
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.pGeometryDescs = build.geometry.data();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild{};
		device->Native()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

		BufferDescription description{
			.updateRate = ResourceFrequency::Static,
			.bindFlags = BindFlag::AccelerationStructure,
			.accessFlags = AccessFlag::GPUWrite,
			.size = prebuild.ResultDataMaxSizeInBytes,
			.stride = 1
		};

		build.blas = resourceManager.Create(description, VGText("Bottom level acceleration structure"));
		build.scratchOffset = scratchBytes;
		scratchBytes += AlignedSize(prebuild.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

		blasCache[key] = build.blas;
		builds->emplace_back(std::move(build));
	});

	if (builds->size() > 0)
	{
		VGLog(logRendering, "Building {} bottom level acceleration structure(s).", builds->size());
	}

	// Second pass: gather TLAS instances. Every mesh has a BLAS in the cache by now.
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;
	meshView.each([&](entt::entity entity, const TransformComponent& transform, const MeshComponent& mesh)
	{
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world, BuildObjectWorldMatrix(transform));

		D3D12_RAYTRACING_INSTANCE_DESC instance{};
		// The engine uses row-vector convention, the instance transform expects column-vector, so transpose.
		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 4; ++column)
			{
				instance.Transform[row][column] = world.m[column][row];
			}
		}

		// Store the GPU scene slot so hit shaders can look up per-object data directly.
		VGAssert(registry.all_of<GpuSlotComponent>(entity), "Entity must has GPU scene setup before acceleration structure building.");
		const auto slot = registry.get<GpuSlotComponent>(entity);
		instance.InstanceID = slot.baseSlot;
		instance.InstanceMask = 0xFF;  // Could reserve a bit for "casts shadows" or such?
		instance.InstanceContributionToHitGroupIndex = 0;  // Unused with inline ray tracing.
		instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instance.AccelerationStructure = resourceManager.Get(blasCache[mesh.globalOffset.position]).Native()->GetGPUVirtualAddress();

		instances.emplace_back(instance);
	});

	if (instances.size() == 0)
	{
		return {};
	}

	// Upload the instance descriptions to this frame's upload buffer.
	EnsureInstanceBuffer(instances.size());
	const auto instanceBuffer = instanceBuffers[device->GetFrameIndex()];
	resourceManager.Write(instanceBuffer, instances);

	// Size the TLAS.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
	tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;  // Rebuilt from scratch every frame.
	tlasInputs.NumDescs = static_cast<UINT>(instances.size());
	tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasInputs.InstanceDescs = resourceManager.Get(instanceBuffer).Native()->GetGPUVirtualAddress();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuild{};
	device->Native()->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuild);

	EnsureTlas(tlasPrebuild.ResultDataMaxSizeInBytes);

	const size_t tlasScratchOffset = scratchBytes;
	scratchBytes += AlignedSize(tlasPrebuild.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
	EnsureScratch(scratchBytes);

	const auto tlasTag = graph.Import(tlas);
	const auto indexBuffer = meshFactory.indexBuffer;

	auto& asPass = graph.AddPass("Acceleration Structure Pass", ExecutionQueue::Compute);
	asPass.Read(meshPositionTag, ResourceBind::SRV);
	asPass.Write(tlasTag, ResourceBind::AS);
	asPass.Bind([this, builds, tlasInputs, tlasScratchOffset, indexBuffer, instanceBuffer](CommandList& list, RenderPassResources& resources)
	{
		auto& resourceManager = device->GetResourceManager();

		// Temporarily take the index buffer out of the index buffer state for the building, restore afterwards
		// since render graph won't do this for us.
		list.TransitionBarrier(indexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		list.TransitionBarrier(scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		list.FlushBarriers();

		const auto scratchAddress = resourceManager.Get(scratch).Native()->GetGPUVirtualAddress();

		for (const auto& build : *builds)
		{
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC description{};
			description.DestAccelerationStructureData = resourceManager.Get(build.blas).Native()->GetGPUVirtualAddress();
			description.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			description.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
			description.Inputs.NumDescs = static_cast<UINT>(build.geometry.size());
			description.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			description.Inputs.pGeometryDescs = build.geometry.data();
			description.ScratchAccelerationStructureData = scratchAddress + build.scratchOffset;

			list.BuildAccelerationStructure(description);
			list.UAVBarrier(build.blas);
		}

		// All BLAS builds must complete before the TLAS build consumes them.
		list.FlushBarriers();

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDescription{};
		tlasDescription.DestAccelerationStructureData = resourceManager.Get(tlas).Native()->GetGPUVirtualAddress();
		tlasDescription.Inputs = tlasInputs;
		tlasDescription.ScratchAccelerationStructureData = scratchAddress + tlasScratchOffset;

		list.BuildAccelerationStructure(tlasDescription);

		list.TransitionBarrier(indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		list.FlushBarriers();
	});

	return AccelerationStructureResources{
		.valid = true,
		.tlasTag = tlasTag
	};
}
