// Copyright (c) 2019-2022 Andrew Depke

#include <Editor/Picking.h>

#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>

#include <algorithm>
#include <limits>

using namespace DirectX;

// #TODO: Consolidate into a common header.
float RaySphereIntersect(XMVECTOR origin, XMVECTOR direction, XMVECTOR sphereCenter, float sphereRadius)
{
	const XMVECTOR offset = XMVectorSubtract(origin, sphereCenter);
	const float b = XMVectorGetX(XMVector3Dot(direction, offset));
	const float c = XMVectorGetX(XMVector3Dot(offset, offset)) - sphereRadius * sphereRadius;

	// Origin inside the sphere counts as a hit at t = 0.
	if (c <= 0.f)
	{
		return 0.f;
	}

	// Sphere is entirely behind the ray.
	if (b > 0.f)
	{
		return -1.f;
	}

	const float discriminant = b * b - c;
	if (discriminant < 0.f)
	{
		return -1.f;
	}

	return -b - std::sqrt(discriminant);
}

namespace Picking
{
	void ProjectUIToWorld(float mouseX, float mouseY,
		float viewportWidth, float viewportHeight,
		float uvCropX, float uvCropY,
		const XMMATRIX& view, const XMMATRIX& projection,
		XMVECTOR& origin, XMVECTOR& direction)
	{
		// Pixel -> UV inside the (possibly larger) render target. The scene image samples the
		// render target with the [uvCrop, 1 - uvCrop] window, so remap accordingly.
		const float u = uvCropX + (mouseX / viewportWidth) * (1.f - 2.f * uvCropX);
		const float v = uvCropY + (mouseY / viewportHeight) * (1.f - 2.f * uvCropY);

		// UV -> NDC.
		const float ndcX = u * 2.f - 1.f;
		const float ndcY = -(v * 2.f - 1.f);

		// Two NDC points spanning the full depth range. Inverse-Z does not matter here: whichever
		// of z=0 / z=1 corresponds to the near plane will be produced by the inverse of the same
		// matrix that put the scene there, so the ray direction will be correct either way.
		const XMVECTOR nearPointNDC = XMVectorSet(ndcX, ndcY, 0.f, 1.f);
		const XMVECTOR farPointNDC  = XMVectorSet(ndcX, ndcY, 1.f, 1.f);

		const XMMATRIX viewProjection = XMMatrixMultiply(view, projection);
		const XMMATRIX invViewProjection = XMMatrixInverse(nullptr, viewProjection);

		// TransformCoord performs the perspective divide, giving us proper world-space points.
		const XMVECTOR nearWorld = XMVector3TransformCoord(nearPointNDC, invViewProjection);
		const XMVECTOR farWorld  = XMVector3TransformCoord(farPointNDC,  invViewProjection);

		origin = nearWorld;
		direction = XMVector3Normalize(XMVectorSubtract(farWorld, nearWorld));
	}

	entt::entity Pick(const entt::registry& registry, XMVECTOR origin, XMVECTOR direction)
	{
		entt::entity hit = entt::null;
		float bestT = std::numeric_limits<float>::max();

		const auto view = registry.view<const TransformComponent, const MeshComponent>();
		view.each([&](auto entity, const TransformComponent& transform, const MeshComponent& mesh)
		{
			const XMVECTOR center = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);
			const float maxScale = std::max(std::max(transform.scale.x, transform.scale.y), transform.scale.z);

			// Search against all subset bounding boxes to find the best match for the entity.
			for (const auto& subset : mesh.subsets)
			{
				const float radius = subset.boundingSphereRadius * maxScale;
				const float t = RaySphereIntersect(origin, direction, center, radius);
				if (t >= 0.f && t < bestT)
				{
					bestT = t;
					hit = entity;
				}
			}
		});

		return hit;
	}
}
