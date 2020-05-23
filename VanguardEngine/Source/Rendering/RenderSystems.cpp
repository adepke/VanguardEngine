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

XMMATRIX SpectatorCameraView(TransformComponent& Transform, const CameraComponent& Camera, float DeltaPitch, float DeltaYaw,
	bool MoveForward, bool MoveBackward, bool MoveLeft, bool MoveRight, bool MoveUp, bool MoveDown)
{
	constexpr auto MovementSpeed = 0.4f;  // #TODO: Multiply by delta time.
	constexpr auto RotationSpeed = 0.4f;

	// #TODO: Pitch locking.
	Transform.Rotation.y += DeltaPitch * RotationSpeed * -1.f;
	Transform.Rotation.z += DeltaYaw * RotationSpeed;

	const auto RotationMatrix = XMMatrixRotationX(-Transform.Rotation.x) * XMMatrixRotationY(-Transform.Rotation.y) * XMMatrixRotationZ(-Transform.Rotation.z);

	const auto Forward = XMVector3TransformCoord(XMVectorSet(1.f, 0.f, 0.f, 0.f), RotationMatrix);
	const auto Upward = XMVector3TransformCoord(XMVectorSet(0.f, 0.f, 1.f, 0.f), RotationMatrix);
	const auto Across = XMVector3Cross(Upward, Forward);

	const auto ForwardMovement = (MoveForward ? 1.f : 0.f) - (MoveBackward ? 1.f : 0.f);
	const auto UpMovement = (MoveUp ? 1.f : 0.f) - (MoveDown ? 1.f : 0.f);
	const auto LeftMovement = (MoveLeft ? 1.f : 0.f) - (MoveRight ? 1.f : 0.f);

	auto EyePosition = XMVectorSet(Transform.Translation.x, Transform.Translation.y, Transform.Translation.z, 0.f);
	EyePosition += Forward * ForwardMovement * MovementSpeed;
	EyePosition += Upward * UpMovement * MovementSpeed;
	EyePosition += Across * LeftMovement * MovementSpeed;

	XMStoreFloat3(&Transform.Translation, EyePosition);

	const auto FocusDirection = XMVector3Normalize(XMVector3TransformCoord(XMVectorSet(1.f, 0.f, 0.f, 0.f), RotationMatrix));
	const auto FocusPosition = EyePosition + FocusDirection;

	return XMMatrixLookAtRH(EyePosition, FocusPosition, Upward);
}

void CameraSystem::Update(entt::registry& Registry)
{
	float PitchDelta = 0.f;
	float YawDelta = 0.f;
	bool MoveForward = false;
	bool MoveBackward = false;
	bool MoveLeft = false;
	bool MoveRight = false;
	bool MoveUp = false;
	bool MoveDown = false;

	auto& IO = ImGui::GetIO();
	if (IO.MouseDown[1])  // Require right click to be pressed in order to move the camera.
	{
		PitchDelta = IO.MouseDelta.y * 0.005f;
		YawDelta = IO.MouseDelta.x * 0.005f;

		if (IO.KeysDown[0x57]) MoveForward = true;  // W
		if (IO.KeysDown[0x53]) MoveBackward = true;  // S
		if (IO.KeysDown[0x41]) MoveLeft = true;  // A
		if (IO.KeysDown[0x44]) MoveRight = true;  // D
		if (IO.KeysDown[VK_SPACE]) MoveUp = true;  // Spacebar
		if (IO.KeysDown[VK_CONTROL]) MoveDown = true;  // Ctrl
	}
	
	// Iterate all camera entities that have control.
	Registry.view<TransformComponent, const CameraComponent, const ControlComponent>().each([&](auto Entity, auto& Transform, const auto& Camera)
		{
			auto ViewMatrix = SpectatorCameraView(Transform, Camera, PitchDelta, YawDelta, MoveForward, MoveBackward, MoveLeft, MoveRight, MoveUp, MoveDown);

			const auto AspectRatio = static_cast<float>(Renderer::Get().Device->RenderWidth) / static_cast<float>(Renderer::Get().Device->RenderHeight);
			const auto ProjectionMatrix = XMMatrixPerspectiveFovRH(Camera.FieldOfView / 2.f, AspectRatio, Camera.NearPlane, Camera.FarPlane);
			
			// #TODO: Support multiple cameras.
			GlobalViewMatrix = ViewMatrix;
			GlobalProjectionMatrix = ProjectionMatrix;
		});
}