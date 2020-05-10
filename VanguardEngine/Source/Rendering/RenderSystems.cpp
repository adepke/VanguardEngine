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
	constexpr auto MovementSpeed = 1.f;  // #TODO: Multiply by delta time.
	constexpr auto RotationSpeed = 4.f;

	const auto FocusPosX = Transform.Translation.x + std::cos(Transform.Rotation.x);
	const auto FocusPosY = Transform.Translation.y + std::sin(Transform.Rotation.y);
	const auto FocusPosZ = Transform.Translation.z + std::sin(Transform.Rotation.x);

	auto EyePosition = XMVectorSet(Transform.Translation.x, Transform.Translation.y, Transform.Translation.z, 0.f);
	auto FocusPosition = XMVectorSet(FocusPosX, FocusPosY, FocusPosZ, 0.f);
	const auto UpDirection = XMVectorSet(0.f, 0.f, 1.f, 0.f);  // Z-up.

	const auto Across = XMVector3Normalize(XMVector3Cross(XMVector3Normalize(FocusPosition), UpDirection));
	const auto Forward = XMVector3Normalize(FocusPosition);
	const auto Upward = XMVector3Normalize(XMVector3Cross(Across, Forward));

	// Movement in the XY plane.
	if ((MoveRight && !MoveLeft) || (!MoveRight && MoveLeft) || (MoveForward && !MoveBackward) || (!MoveForward && MoveBackward))
	{
		const auto MultiplierX = (MoveForward ? 1.f : 0.f) - (MoveBackward ? 1.f : 0.f);
		const auto MultiplierY = (MoveLeft ? 1.f : 0.f) - (MoveRight ? 1.f : 0.f);

		XMFLOAT3 AcrossUnpacked;
		XMFLOAT3 ForwardUnpacked;

		XMStoreFloat3(&AcrossUnpacked, Across);
		XMStoreFloat3(&ForwardUnpacked, Forward);

		auto MovementXY = XMVectorSet(
			ForwardUnpacked.x * MultiplierX + AcrossUnpacked.x * MultiplierY,
			ForwardUnpacked.y * MultiplierX + AcrossUnpacked.y * MultiplierY,
			ForwardUnpacked.z * MultiplierX + AcrossUnpacked.z * MultiplierY,
			0.f
		);

		MovementXY = XMVector3Normalize(MovementXY);

		XMFLOAT3 MovementXYUnpacked;
		XMStoreFloat3(&MovementXYUnpacked, MovementXY);

		XMFLOAT3 EyePositionUnpacked;
		XMStoreFloat3(&EyePositionUnpacked, EyePosition);

		EyePositionUnpacked.x += MovementXYUnpacked.x * MovementSpeed;
		EyePositionUnpacked.y += MovementXYUnpacked.y * MovementSpeed;
		EyePositionUnpacked.z += MovementXYUnpacked.z * MovementSpeed;

		EyePosition = XMLoadFloat3(&EyePositionUnpacked);
	}

	// Movement along the Z axis.
	if ((MoveUp && !MoveDown) || (!MoveUp && MoveDown))
	{
		const auto MultiplierZ = (MoveUp ? 1.f : 0.f) - (MoveDown ? 1.f : 0.f);

		XMFLOAT3 UpDirectionUnpacked;
		XMStoreFloat3(&UpDirectionUnpacked, UpDirection);

		auto MovementZ = XMVectorSet(UpDirectionUnpacked.x * MultiplierZ, UpDirectionUnpacked.y * MultiplierZ, UpDirectionUnpacked.z * MultiplierZ, 0.f);
		MovementZ = XMVector3Normalize(MovementZ);

		XMFLOAT3 MovementZUnpacked;
		XMStoreFloat3(&MovementZUnpacked, MovementZ);

		XMFLOAT3 EyePositionUnpacked;
		XMStoreFloat3(&EyePositionUnpacked, EyePosition);

		EyePositionUnpacked.x += MovementZUnpacked.x * MovementSpeed;
		EyePositionUnpacked.y += MovementZUnpacked.y * MovementSpeed;
		EyePositionUnpacked.z += MovementZUnpacked.z * MovementSpeed;

		EyePosition = XMLoadFloat3(&EyePositionUnpacked);
	}

	// Yaw rotation.
	if (DeltaYaw != 0.f)
	{
		const auto YawRads = -DeltaYaw * RotationSpeed;
		const auto YawSine = std::sin(YawRads);
		const auto YawCosine = std::cos(YawRads);

		XMFLOAT3 UpDirectionUnpacked;
		XMStoreFloat3(&UpDirectionUnpacked, UpDirection);

		XMFLOAT3X3 RotationMatrix{
			YawCosine + (1.f - YawCosine) * UpDirectionUnpacked.x * UpDirectionUnpacked.x,
			(1.f - YawCosine) * UpDirectionUnpacked.x * UpDirectionUnpacked.y + YawSine * UpDirectionUnpacked.z,
			(1.f - YawCosine) * UpDirectionUnpacked.x * UpDirectionUnpacked.z - YawSine * UpDirectionUnpacked.y,
			(1.f - YawCosine) * UpDirectionUnpacked.x * UpDirectionUnpacked.y - YawSine * UpDirectionUnpacked.z,
			YawCosine + (1.f - YawCosine) * UpDirectionUnpacked.y * UpDirectionUnpacked.y,
			(1.f - YawCosine) * UpDirectionUnpacked.y * UpDirectionUnpacked.z + YawSine * UpDirectionUnpacked.x,
			(1.f - YawCosine) * UpDirectionUnpacked.x * UpDirectionUnpacked.z + YawSine * UpDirectionUnpacked.y,
			(1.f - YawCosine) * UpDirectionUnpacked.y * UpDirectionUnpacked.z - YawSine * UpDirectionUnpacked.x,
			YawCosine + (1.f - YawCosine) * UpDirectionUnpacked.z * UpDirectionUnpacked.z
		};

		XMFLOAT3 FocusPositionUnpacked;
		XMStoreFloat3(&FocusPositionUnpacked, FocusPosition);

		auto NewFocusPosition = XMVectorSet(
			RotationMatrix.m[0][0] * FocusPositionUnpacked.x + RotationMatrix.m[1][0] * FocusPositionUnpacked.y + RotationMatrix.m[2][0] * FocusPositionUnpacked.z,
			RotationMatrix.m[0][1] * FocusPositionUnpacked.x + RotationMatrix.m[1][1] * FocusPositionUnpacked.y + RotationMatrix.m[2][1] * FocusPositionUnpacked.z,
			RotationMatrix.m[0][2] * FocusPositionUnpacked.x + RotationMatrix.m[1][2] * FocusPositionUnpacked.y + RotationMatrix.m[2][2] * FocusPositionUnpacked.z,
			0.f
		);

		FocusPosition = XMVector3Normalize(NewFocusPosition);
	}

	// Pitch rotation.
	if (DeltaPitch != 0.f)
	{
		XMFLOAT3 FocusPositionUnpacked;
		XMFLOAT3 UpDirectionUnpacked;
		XMFLOAT3 AcrossUnpacked;

		XMStoreFloat3(&FocusPositionUnpacked, FocusPosition);
		XMStoreFloat3(&UpDirectionUnpacked, UpDirection);
		XMStoreFloat3(&AcrossUnpacked, Across);

		const auto RadiansToUp = std::acos(FocusPositionUnpacked.x * UpDirectionUnpacked.x + FocusPositionUnpacked.y * UpDirectionUnpacked.y + FocusPositionUnpacked.z * UpDirectionUnpacked.z);
		const auto RadiansToDown = std::acos(FocusPositionUnpacked.x * -UpDirectionUnpacked.x + FocusPositionUnpacked.y * -UpDirectionUnpacked.y + FocusPositionUnpacked.z * -UpDirectionUnpacked.z);

		const auto DegreesToUp = RadiansToUp / 3.14159265359f * 180.f;
		const auto DegreesToDown = RadiansToDown / 3.14159265359f * 180.f;

		auto MaxPitchDegrees = DegreesToUp - 0.000001f;  // Offset slightly from 90 degrees.
		if (MaxPitchDegrees < 0.f)
			MaxPitchDegrees = 0.f;

		auto MinPitchDegrees = DegreesToDown - 0.000001f;  // Offset slightly from 90 degrees.
		if (MinPitchDegrees < 0.f)
			MinPitchDegrees = 0.f;

		auto PitchRads = DeltaPitch * RotationSpeed;
		auto PitchDegrees = PitchRads / 3.14159265359f * 180.f;

		PitchDegrees = std::min(PitchDegrees, MaxPitchDegrees);
		PitchDegrees = std::max(PitchDegrees, -MinPitchDegrees);

		PitchRads = PitchDegrees * 3.14159265359f / 180.f;
		const auto PitchSine = std::sin(PitchRads);
		const auto PitchCosine = std::cos(PitchRads);

		XMFLOAT3X3 RotationMatrix{
			PitchCosine + (1.f - PitchCosine) * AcrossUnpacked.x * AcrossUnpacked.x,
			(1.f - PitchCosine) * AcrossUnpacked.x * AcrossUnpacked.y + PitchSine * AcrossUnpacked.z,
			(1.f - PitchCosine) * AcrossUnpacked.x * AcrossUnpacked.z - PitchSine * AcrossUnpacked.y,
			(1.f - PitchCosine) * AcrossUnpacked.x * AcrossUnpacked.y - PitchSine * AcrossUnpacked.z,
			PitchCosine + (1.f - PitchCosine) * AcrossUnpacked.y * AcrossUnpacked.y,
			(1.f - PitchCosine) * AcrossUnpacked.y * AcrossUnpacked.z + PitchSine * AcrossUnpacked.x,
			(1.f - PitchCosine) * AcrossUnpacked.x * AcrossUnpacked.z + PitchSine * AcrossUnpacked.y,
			(1.f - PitchCosine) * AcrossUnpacked.y * AcrossUnpacked.z - PitchSine * AcrossUnpacked.x,
			PitchCosine + (1.f - PitchCosine) * AcrossUnpacked.z * AcrossUnpacked.z
		};

		auto NewFocusPosition = XMVectorSet(
			RotationMatrix.m[0][0] * FocusPositionUnpacked.x + RotationMatrix.m[1][0] * FocusPositionUnpacked.y + RotationMatrix.m[2][0] * FocusPositionUnpacked.z,
			RotationMatrix.m[0][1] * FocusPositionUnpacked.x + RotationMatrix.m[1][1] * FocusPositionUnpacked.y + RotationMatrix.m[2][1] * FocusPositionUnpacked.z,
			RotationMatrix.m[0][2] * FocusPositionUnpacked.x + RotationMatrix.m[1][2] * FocusPositionUnpacked.y + RotationMatrix.m[2][2] * FocusPositionUnpacked.z,
			0.f
		);

		FocusPosition = XMVector3Normalize(NewFocusPosition);
	}

	XMStoreFloat3(&Transform.Translation, EyePosition);

	const auto LookDirection = XMVector3Normalize(FocusPosition - EyePosition);

	XMFLOAT3 LookDirectionUnpacked;
	XMStoreFloat3(&LookDirectionUnpacked, LookDirection);

	Transform.Rotation.x = std::acos(LookDirectionUnpacked.x);
	Transform.Rotation.y = std::asin(LookDirectionUnpacked.y);
	Transform.Rotation.z = std::asin(LookDirectionUnpacked.z);

	return XMMatrixLookAtRH(EyePosition, FocusPosition, UpDirection);
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
		//PitchDelta = IO.MouseDelta.y * 0.005f;
		//YawDelta = IO.MouseDelta.x * 0.005f;

		if (IO.KeysDown[0x57]) MoveForward = true;  // W
		if (IO.KeysDown[0x53]) MoveBackward = true;  // S
		if (IO.KeysDown[0x41]) MoveLeft = true;  // A
		if (IO.KeysDown[0x44]) MoveRight = true;  // D
		if (IO.KeysDown[VK_SPACE]) MoveUp = true;  // Spacebar
		if (IO.KeysDown[VK_CONTROL]) MoveDown = true;  // Ctrl
	}

	// #TODO: Movement affects all cameras in the registry, only affect the controlled camera.

	Registry.view<TransformComponent, const CameraComponent>().each([&](auto Entity, auto& Transform, const auto& Camera)
		{
			auto ViewMatrix = SpectatorCameraView(Transform, Camera, PitchDelta, YawDelta, MoveForward, MoveBackward, MoveLeft, MoveRight, MoveUp, MoveDown);

			const auto AspectRatio = static_cast<float>(Renderer::Get().Device->RenderWidth) / static_cast<float>(Renderer::Get().Device->RenderHeight);
			const auto ProjectionMatrix = XMMatrixPerspectiveFovRH(Camera.FieldOfView / 2.f, AspectRatio, Camera.NearPlane, Camera.FarPlane);
			
			// #TODO: Support multiple cameras.
			GlobalViewMatrix = ViewMatrix;
			GlobalProjectionMatrix = ProjectionMatrix;
		});
}