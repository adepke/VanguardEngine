// Copyright (c) 2019-2021 Andrew Depke

#include <Rendering/RenderSystems.h>
#include <Rendering/Base.h>
#include <Rendering/Renderer.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/ShaderStructs.h>

#include <imgui.h>

#include <cmath>
#include <algorithm>

void MeshSystem::Render(Renderer& renderer, const entt::registry& registry, CommandList& list, bool renderTransparents)
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
		list.BindResource("perObject", renderer.instanceBuffer, renderer.instanceOffset + (index * sizeof(EntityInstance)));

		std::vector<uint8_t> metadata;
		metadata.resize(sizeof(VertexMetadata));
		std::memcpy(metadata.data(), &mesh.metadata, metadata.size());
		const auto [metadataBuffer, metadataOffset] = renderer.device->FrameAllocate(sizeof(VertexMetadata));
		renderer.device->GetResourceManager().Write(metadataBuffer, metadata, metadataOffset);
		list.BindResource("vertexMetadata", metadataBuffer, metadataOffset);

		for (const auto& subset : mesh.subsets)
		{
			if (!renderTransparents && subset.material.transparent)
				continue;

			if (renderer.device->GetResourceManager().Valid(subset.material.materialBuffer))
			{
				list.BindResourceOptional("material", subset.material.materialBuffer);
			}

			list.BindResource("vertexPositionBuffer", renderer.meshFactory->vertexPositionBuffer, mesh.globalOffset.position + subset.localOffset.position);
			list.BindResourceOptional("vertexExtraBuffer", renderer.meshFactory->vertexExtraBuffer, mesh.globalOffset.extra + subset.localOffset.extra);

			list.Native()->DrawIndexedInstanced(static_cast<uint32_t>(subset.indices), 1, (mesh.globalOffset.index + subset.localOffset.index) / sizeof(uint32_t), 0, 0);
		}

		++index;
	});
}

XMMATRIX SpectatorCameraView(TransformComponent& transform, const CameraComponent& camera, float deltaTime, float deltaPitch, float deltaYaw,
	bool moveForward, bool moveBackward, bool moveLeft, bool moveRight, bool moveUp, bool moveDown, bool moveSprint)
{
	VGScopedCPUStat("Spectator Camera View");

	const auto movementSpeed = 25.f * (moveSprint ? 3.f : 1.f) * deltaTime;
	constexpr auto rotationSpeed = 0.4f;

	transform.rotation.y += deltaPitch * rotationSpeed * -1.f;
	transform.rotation.z += deltaYaw * rotationSpeed;

	constexpr auto maxPitch = 89.999999f * 3.14159265359f / 180.f;
	transform.rotation.y = std::clamp(transform.rotation.y, maxPitch * -1.f, maxPitch);

	const auto rotationMatrix = XMMatrixRotationX(-transform.rotation.x) * XMMatrixRotationY(-transform.rotation.y) * XMMatrixRotationZ(-transform.rotation.z);

	const auto forward = XMVector4Transform(XMVectorSet(1.f, 0.f, 0.f, 0.f), rotationMatrix);
	const auto upward = XMVector4Transform(XMVectorSet(0.f, 0.f, 1.f, 0.f), rotationMatrix);
	const auto across = XMVector3Cross(upward, forward);

	const auto forwardMovement = (moveForward ? 1.f : 0.f) - (moveBackward ? 1.f : 0.f);
	const auto upMovement = (moveUp ? 1.f : 0.f) - (moveDown ? 1.f : 0.f);
	const auto leftMovement = (moveLeft ? 1.f : 0.f) - (moveRight ? 1.f : 0.f);

	auto eyePosition = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);
	eyePosition += forward * forwardMovement * movementSpeed;
	eyePosition += XMVectorSet(0.f, 0.f, 1.f, 0.f) * upMovement * movementSpeed;  // Upward movement is not relative to the camera rotation.
	eyePosition += across * leftMovement * movementSpeed;

	XMStoreFloat3(&transform.translation, eyePosition);

	return XMMatrixLookAtRH(eyePosition, eyePosition + forward, upward);
}

void CameraSystem::Update(entt::registry& registry, float deltaTime)
{
	VGScopedCPUStat("Camera System");

	bool moveForward = false;
	bool moveBackward = false;
	bool moveLeft = false;
	bool moveRight = false;
	bool moveUp = false;
	bool moveDown = false;
	bool moveSprint = false;

	auto& io = ImGui::GetIO();
	const auto pitchDelta = io.MouseDelta.y * 0.005f;
	const auto yawDelta = io.MouseDelta.x * 0.005f;

	// #TODO: Use ImGui::IsKeyDown or ImGui::IsKeyPressed here.
	if (io.KeysDown[0x57]) moveForward = true;  // W
	if (io.KeysDown[0x53]) moveBackward = true;  // S
	if (io.KeysDown[0x41]) moveLeft = true;  // A
	if (io.KeysDown[0x44]) moveRight = true;  // D
	if (io.KeysDown[VK_SPACE]) moveUp = true;  // Spacebar
	if (io.KeysDown[VK_CONTROL]) moveDown = true;  // Ctrl
	if (io.KeysDown[VK_SHIFT]) moveSprint = true;  // Shift
	
	// Iterate all camera entities that have control.
	registry.view<TransformComponent, const CameraComponent, const ControlComponent>().each([&](auto entity, auto& transform, const auto& camera)
	{
		auto viewMatrix = SpectatorCameraView(transform, camera, deltaTime, pitchDelta, yawDelta, moveForward, moveBackward, moveLeft, moveRight, moveUp, moveDown, moveSprint);

		const auto aspectRatio = static_cast<float>(Renderer::Get().device->renderWidth) / static_cast<float>(Renderer::Get().device->renderHeight);
		const auto projectionMatrix = XMMatrixPerspectiveFovRH(camera.fieldOfView / 2.f, aspectRatio, camera.farPlane, camera.nearPlane);  // Inverse Z.
		
		// #TODO: Support multiple cameras.
		globalViewMatrix = viewMatrix;
		globalProjectionMatrix = projectionMatrix;
	});
}