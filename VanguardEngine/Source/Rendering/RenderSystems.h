// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/Renderer.h>
#include <Rendering/CommandList.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <entt/entt.hpp>
#include <Rendering/ShaderStructs.h>
#include <Utility/Reflection.h>

class Renderer;
class CommandList;

struct MeshSystem
{
	template <typename T>
	static void Render(Renderer& renderer, const entt::registry& registry, CommandList& list, T& bindData);
};

struct CameraSystem
{
	static void Update(entt::registry& registry, float deltaTime);
};

// Hacky macro-based reflection solution since std::reflect isn't out yet.
VGMakeMemberCheck(materialIndex);

// #TODO: Find a better solution than making this a template.

template <typename T>
void MeshSystem::Render(Renderer& renderer, const entt::registry& registry, CommandList& list, T& bindData)
{
	auto& indexBuffer = renderer.device->GetResourceManager().Get(renderer.meshFactory->indexBuffer);
	D3D12_INDEX_BUFFER_VIEW indexView{
		.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(indexBuffer.description.size * indexBuffer.description.stride),
		.Format = DXGI_FORMAT_R32_UINT
	};
	list.Native()->IASetIndexBuffer(&indexView);

	size_t index = 0;
	registry.view<const TransformComponent, const MeshComponent>().each([&](auto entity, const auto&, const auto& mesh)
	{
		for (const auto& subset : mesh.subsets)
		{
			bindData.objectIndex = index;
			bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.

			if constexpr (VGHasMember(T, materialIndex))
			{
				bindData.materialIndex = subset.materialIndex;
			}

			bindData.vertexAssemblyData.metadata = mesh.metadata;
			// Add local and global offsets.
			const auto positionOffset = bindData.vertexAssemblyData.metadata.channelOffsets[0][0];  // Save the old position offset.
			for (int i = 0; i < vertexChannels / 4 + 1; ++i)
			{
				bindData.vertexAssemblyData.metadata.channelOffsets[i].AddAll(mesh.globalOffset.extra + subset.localOffset.extra);
			}
			bindData.vertexAssemblyData.metadata.channelOffsets[0][0] = positionOffset + mesh.globalOffset.position + subset.localOffset.position;

			list.BindConstants("bindData", bindData);

			list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, (mesh.globalOffset.index + subset.localOffset.index) / sizeof(uint32_t), 0, 0);
		}

		++index;
	});
}