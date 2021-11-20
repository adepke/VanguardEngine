// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <entt/entt.hpp>

class Renderer;
class CommandList;

struct MeshSystem
{
	static void Render(Renderer& renderer, const entt::registry& registry, CommandList& list, bool renderTransparents);
};

struct CameraSystem
{
	static void Update(entt::registry& registry, float deltaTime);
};