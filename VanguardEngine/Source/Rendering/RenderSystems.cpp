// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderSystems.h>
#include <Rendering/Base.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/Renderer.h>

#include <imgui.h>

#include <cmath>
#include <algorithm>

#include <DirectXMath.h>

XMMATRIX SpectatorCameraView(TransformComponent& transform, const CameraComponent& camera, float deltaPitch, float deltaYaw,
	bool moveForward, bool moveBackward, bool moveLeft, bool moveRight, bool moveUp, bool moveDown)
{
	VGScopedCPUStat("Spectator Camera View");

	constexpr auto movementSpeed = 0.5f;  // #TODO: Multiply by delta time.
	constexpr auto rotationSpeed = 0.4f;

	// #TODO: Pitch locking.
	transform.rotation.y += deltaPitch * rotationSpeed * -1.f;
	transform.rotation.z += deltaYaw * rotationSpeed;

	const auto rotationMatrix = XMMatrixRotationX(-transform.rotation.x) * XMMatrixRotationY(-transform.rotation.y) * XMMatrixRotationZ(-transform.rotation.z);

	const auto forward = XMVector3TransformCoord(XMVectorSet(1.f, 0.f, 0.f, 0.f), rotationMatrix);
	const auto upward = XMVector3TransformCoord(XMVectorSet(0.f, 0.f, 1.f, 0.f), rotationMatrix);
	const auto across = XMVector3Cross(upward, forward);

	const auto forwardMovement = (moveForward ? 1.f : 0.f) - (moveBackward ? 1.f : 0.f);
	const auto upMovement = (moveUp ? 1.f : 0.f) - (moveDown ? 1.f : 0.f);
	const auto leftMovement = (moveLeft ? 1.f : 0.f) - (moveRight ? 1.f : 0.f);

	auto eyePosition = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);
	eyePosition += forward * forwardMovement * movementSpeed;
	eyePosition += upward * upMovement * movementSpeed;
	eyePosition += across * leftMovement * movementSpeed;

	XMStoreFloat3(&transform.translation, eyePosition);

	const auto focusDirection = XMVector3Normalize(XMVector3TransformCoord(XMVectorSet(1.f, 0.f, 0.f, 0.f), rotationMatrix));
	const auto focusPosition = eyePosition + focusDirection;

	return XMMatrixLookAtRH(eyePosition, focusPosition, upward);
}

void CameraSystem::Update(entt::registry& registry)
{
	VGScopedCPUStat("Camera System");

	bool moveForward = false;
	bool moveBackward = false;
	bool moveLeft = false;
	bool moveRight = false;
	bool moveUp = false;
	bool moveDown = false;

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
	
	// Iterate all camera entities that have control.
	registry.view<TransformComponent, const CameraComponent, const ControlComponent>().each([&](auto entity, auto& transform, const auto& camera)
		{
			auto viewMatrix = SpectatorCameraView(transform, camera, pitchDelta, yawDelta, moveForward, moveBackward, moveLeft, moveRight, moveUp, moveDown);

			const auto aspectRatio = static_cast<float>(Renderer::Get().device->renderWidth) / static_cast<float>(Renderer::Get().device->renderHeight);
			const auto projectionMatrix = XMMatrixPerspectiveFovRH(camera.fieldOfView / 2.f, aspectRatio, camera.nearPlane, camera.farPlane);
			
			// #TODO: Support multiple cameras.
			globalViewMatrix = viewMatrix;
			globalProjectionMatrix = projectionMatrix;
		});
}