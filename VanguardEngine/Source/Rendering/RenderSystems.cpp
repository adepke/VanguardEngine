// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/RenderSystems.h>
#include <Rendering/Base.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Rendering/Renderer.h>

#include <DirectXMath.h>

void CameraSystem::Update(entt::registry& Registry)
{
	Registry.view<const TransformComponent, const CameraComponent>().each([](auto Entity, const auto& Transform, const auto& Camera)
		{
			const auto EyePosition = XMVectorSet(Transform.Translation.x, Transform.Translation.y, Transform.Translation.z, 1.f);
			const auto FocusPosition = XMVectorSet(Camera.FocusPosition.x, Camera.FocusPosition.y, Camera.FocusPosition.z, 1.f);
			const auto UpDirection = XMVectorSet(Camera.UpDirection.x, Camera.UpDirection.y, Camera.UpDirection.z, 1.f);
			const auto AspectRatio = static_cast<float>(Renderer::Get().Device->RenderWidth) / static_cast<float>(Renderer::Get().Device->RenderHeight);

			const auto ViewMatrix = XMMatrixLookAtRH(EyePosition, FocusPosition, UpDirection);
			const auto ProjectionMatrix = XMMatrixPerspectiveFovRH(Camera.FieldOfView, AspectRatio, Camera.NearPlane, Camera.FarPlane);

			// #TODO: Support multiple cameras.
			GlobalViewMatrix = ViewMatrix;
			GlobalProjectionMatrix = ProjectionMatrix;
		});
}