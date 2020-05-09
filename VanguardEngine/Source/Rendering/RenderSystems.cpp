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
			const auto EyePosition = XMVectorSet(Transform.Translation.x, Transform.Translation.y, Transform.Translation.z, 0.f);
			const auto FocusPosition = XMVectorSet(Camera.FocusPosition.x, Camera.FocusPosition.y, Camera.FocusPosition.z, 0.f);
			const auto UpDirection = XMVectorSet(0.f, 0.f, 1.f, 0.f);  // Z-up.
			const auto AspectRatio = static_cast<float>(Renderer::Get().Device->RenderWidth) / static_cast<float>(Renderer::Get().Device->RenderHeight);

			const auto ViewMatrix = XMMatrixLookAtRH(EyePosition, FocusPosition, UpDirection);
			const auto ProjectionMatrix = XMMatrixPerspectiveFovRH(Camera.FieldOfView / 2.f, AspectRatio, Camera.NearPlane, Camera.FarPlane);
			
			// #TODO: Support multiple cameras.
			GlobalViewMatrix = ViewMatrix;
			GlobalProjectionMatrix = ProjectionMatrix;
		});
}