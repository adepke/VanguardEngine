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
	static void Render(Renderer& renderer, const entt::registry& registry, CommandList& list, T& bindData, BufferHandle indirectRenderArgs);
};

struct CameraSystem
{
	static void Update(entt::registry& registry, float deltaTime);
};

struct TimeOfDaySystem
{
	static void Update(entt::registry& registry, float deltaTime);
};

// Hacky macro-based reflection solution since std::reflect isn't out yet.
VGMakeMemberCheck(materialIndex);

// #TODO: Find a better solution than making this a template.

template <typename T>
void MeshSystem::Render(Renderer& renderer, const entt::registry& registry, CommandList& list, T& bindData, BufferHandle indirectRenderArgs)
{
	auto& indexBuffer = renderer.device->GetResourceManager().Get(renderer.meshFactory->indexBuffer);
	D3D12_INDEX_BUFFER_VIEW indexView{
		.BufferLocation = indexBuffer.Native()->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(indexBuffer.description.size * indexBuffer.description.stride),
		.Format = DXGI_FORMAT_R32_UINT
	};
	list.Native()->IASetIndexBuffer(&indexView);

	bindData.cameraIndex = 0;  // #TODO: Support multiple cameras.
	list.BindConstants("bindData", bindData);

	auto& indirectBuffer = Renderer::Get().device->GetResourceManager().Get(indirectRenderArgs);
	auto& counterBuffer = Renderer::Get().device->GetResourceManager().Get(indirectBuffer.counterBuffer);

	list.Native()->ExecuteIndirect(Renderer::Get().meshIndirectCommandSignature.Get(), Renderer::Get().renderableCount, indirectBuffer.Native(), 0, counterBuffer.Native(), 0);
}