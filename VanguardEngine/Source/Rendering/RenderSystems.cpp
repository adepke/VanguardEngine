// Copyright (c) 2019-2022 Andrew Depke

#include <Rendering/RenderSystems.h>
#include <Rendering/ShaderStructs.h>

#include <imgui.h>

#include <cmath>
#include <algorithm>
#include <numbers>

void CameraBasis(const TransformComponent& transform, XMVECTOR& forward, XMVECTOR& upward, XMVECTOR& across)
{
	const auto rotationMatrix = XMMatrixRotationX(-transform.rotation.x) * XMMatrixRotationY(-transform.rotation.y) * XMMatrixRotationZ(-transform.rotation.z);

	forward = XMVector4Transform(XMVectorSet(1.f, 0.f, 0.f, 0.f), rotationMatrix);
	upward = XMVector4Transform(XMVectorSet(0.f, 0.f, 1.f, 0.f), rotationMatrix);
	across = XMVector3Cross(upward, forward);
}

XMMATRIX CameraViewMatrix(const TransformComponent& transform)
{
	XMVECTOR forward, upward, across;
	CameraBasis(transform, forward, upward, across);

	const auto eyePosition = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);

	return XMMatrixLookAtRH(eyePosition, eyePosition + forward, upward);
}

// Finds a camera with Control, otherwise a camera.
entt::entity FindActiveCamera(entt::registry& registry)
{
	// #TODO: Support multiple cameras.

	auto controlled = registry.view<const TransformComponent, const CameraComponent, const ControlComponent>();
	if (controlled.begin() != controlled.end())
	{
		return *controlled.begin();
	}

	auto fallback = registry.view<const TransformComponent, const CameraComponent>();
	if (fallback.begin() != fallback.end())
	{
		return *fallback.begin();
	}

	return entt::null;
}

// Polls UI input and applies it to a transform component.
void ApplyMovementInput(TransformComponent& transform, float deltaTime)
{
	bool moveForward = false;
	bool moveBackward = false;
	bool moveLeft = false;
	bool moveRight = false;
	bool moveUp = false;
	bool moveDown = false;
	bool moveSprint = false;

	auto& io = ImGui::GetIO();
	const auto deltaPitch = io.MouseDelta.y * 0.005f;
	const auto deltaYaw = io.MouseDelta.x * 0.005f;

	if (ImGui::IsKeyDown(ImGuiKey_W)) moveForward = true;
	if (ImGui::IsKeyDown(ImGuiKey_S)) moveBackward = true;
	if (ImGui::IsKeyDown(ImGuiKey_A)) moveLeft = true;
	if (ImGui::IsKeyDown(ImGuiKey_D)) moveRight = true;
	if (ImGui::IsKeyDown(ImGuiKey_Space)) moveUp = true;
	if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) moveDown = true;
	if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) moveSprint = true;

	if (auto cvar = CvarGet("cameraSpeed", float); cvar && io.MouseWheel != 0.f)
	{
		constexpr float scrollStepFactor = 1.15f;  // ~15% per scroll.
		constexpr float minCameraSpeed = 0.05f;
		constexpr float maxCameraSpeed = 200.f;
		const float updated = std::clamp(*cvar * std::pow(scrollStepFactor, io.MouseWheel), minCameraSpeed, maxCameraSpeed);
		CvarSet("cameraSpeed", updated);
	}

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

	XMVECTOR forward, upward, across;
	CameraBasis(transform, forward, upward, across);

	const auto forwardMovement = (moveForward ? 1.f : 0.f) - (moveBackward ? 1.f : 0.f);
	const auto upMovement = (moveUp ? 1.f : 0.f) - (moveDown ? 1.f : 0.f);
	const auto leftMovement = (moveLeft ? 1.f : 0.f) - (moveRight ? 1.f : 0.f);

	auto eyePosition = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);
	eyePosition += forward * forwardMovement * movementSpeed;
	eyePosition += XMVectorSet(0.f, 0.f, 1.f, 0.f) * upMovement * movementSpeed;  // Upward movement is not relative to the camera rotation.
	eyePosition += across * leftMovement * movementSpeed;

	XMStoreFloat3(&transform.translation, eyePosition);
}

void CameraSystem::Update(entt::registry& registry, float deltaTime)
{
	VGScopedCPUStat("Camera System");

	// #TODO: don't use a cvar for this.
	CvarCreate("cameraSpeed", "How fast the camera should move", 1.f);

	const auto activeCamera = FindActiveCamera(registry);
	if (activeCamera == entt::null)
	{
		return;
	}

	auto& transform = registry.get<TransformComponent>(activeCamera);
	const auto& camera = registry.get<CameraComponent>(activeCamera);

	// If the camera has control, update with UI input.
	if (registry.all_of<ControlComponent>(activeCamera))
	{
		ApplyMovementInput(transform, deltaTime);

		registry.patch<TransformComponent>(activeCamera);
	}

	const auto viewMatrix = CameraViewMatrix(transform);

	const auto aspectRatio = static_cast<float>(Renderer::Get().device->renderWidth) / static_cast<float>(Renderer::Get().device->renderHeight);
	const auto projectionMatrix = XMMatrixPerspectiveFovRH(camera.fieldOfView / 2.f, aspectRatio, camera.farPlane, camera.nearPlane);  // Inverse Z.

	// #TODO: Support multiple cameras.
	globalLastFrameViewMatrix = globalViewMatrix;
	globalLastFrameProjectionMatrix = globalProjectionMatrix;
	globalViewMatrix = viewMatrix;
	globalProjectionMatrix = projectionMatrix;
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