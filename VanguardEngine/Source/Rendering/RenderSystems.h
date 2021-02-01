// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <entt/entt.hpp>

struct CameraSystem
{
	static void Update(entt::registry& registry, float deltaTime);
};