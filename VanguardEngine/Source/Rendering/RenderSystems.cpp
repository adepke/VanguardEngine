// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/RenderSystems.h>
#include <Rendering/ShaderStructs.h>

#include <imgui.h>

#include <cmath>
#include <algorithm>
#include <numbers>

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

	if (ImGui::IsKeyDown(ImGuiKey_W)) moveForward = true;
	if (ImGui::IsKeyDown(ImGuiKey_S)) moveBackward = true;
	if (ImGui::IsKeyDown(ImGuiKey_A)) moveLeft = true;
	if (ImGui::IsKeyDown(ImGuiKey_D)) moveRight = true;
	if (ImGui::IsKeyDown(ImGuiKey_Space)) moveUp = true;
	if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) moveDown = true;
	if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) moveSprint = true;

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

void TimeOfDaySystem::Update(entt::registry& registry, float deltaTime)
{
	VGScopedCPUStat("Time of Day System");

	registry.view<TimeOfDayComponent>().each([&](auto entity, auto& timeOfDay)
	{
		switch (timeOfDay.animation)
		{
		case TimeOfDayAnimation::Static:
			// Do nothing.
			break;
		case TimeOfDayAnimation::Cycle:
			timeOfDay.solarZenithAngle += timeOfDay.speed * deltaTime * 0.1f;
			timeOfDay.solarZenithAngle = std::fmodf(timeOfDay.solarZenithAngle, 2.f * std::numbers::pi_v<float>);
			break;
		case TimeOfDayAnimation::Oscillate:
			constexpr float threshold = 0.0001f;
			const float direction = timeOfDay.speed / std::fabsf(timeOfDay.speed);

			const float angleDelta = 0.25f * std::cosf(2.f * timeOfDay.solarZenithAngle) + 0.3f;
			timeOfDay.solarZenithAngle += angleDelta * timeOfDay.speed * deltaTime * 0.3f;

			if (std::fabsf(timeOfDay.solarZenithAngle) > std::numbers::pi_v<float> * 0.5f - threshold)
			{
				timeOfDay.speed *= -1.f;  // Invert direction.
				timeOfDay.solarZenithAngle += threshold * timeOfDay.speed;
			}

			break;
		}
	});
}