// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/RenderSystems.h>
#include <Rendering/ShaderStructs.h>

#include <imgui.h>

#include <cmath>
#include <algorithm>

XMMATRIX SpectatorCameraView(TransformComponent& transform, const CameraComponent& camera, float deltaTime, float deltaPitch, float deltaYaw,
	bool moveForward, bool moveBackward, bool moveLeft, bool moveRight, bool moveUp, bool moveDown, bool moveSprint)
{
	VGScopedCPUStat("Spectator Camera View");

	auto movementSpeed = 25.f * (moveSprint ? 3.f : 1.f) * deltaTime;
	constexpr auto rotationSpeed = 0.4f;

	if (auto cvar = CvarGet("cameraSpeed", float); cvar)
	{
		movementSpeed *= *cvar;
	}

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

	CvarCreate("cameraSpeed", "How fast the camera should move", 1.f);

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
		globalLastFrameViewMatrix = globalViewMatrix;
		globalLastFrameProjectionMatrix = globalProjectionMatrix;
		globalViewMatrix = viewMatrix;
		globalProjectionMatrix = projectionMatrix;
	});
}