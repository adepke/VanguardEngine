// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <entt/entt.hpp>

#include <DirectXMath.h>

namespace Picking
{
	// Projects points, like the editor mouse cursor, into a world space ray.
	// UV cropping used to account for cropped scene window display.
	void ProjectUIToWorld(float mouseX, float mouseY,
		float viewportWidth, float viewportHeight,
		float uvCropX, float uvCropY,
		const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection,
		DirectX::XMVECTOR& origin, DirectX::XMVECTOR& direction);

	// Select the entity nearest to the given world space ray. Returns entt:null if none.
	entt::entity Pick(const entt::registry& registry, DirectX::XMVECTOR origin, DirectX::XMVECTOR direction);
}
