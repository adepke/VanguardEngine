// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <entt/entt.hpp>

// Lightweight type safe generational handles for render resources.

struct BufferHandle
{
	entt::entity handle = entt::null;
};

struct TextureHandle
{
	entt::entity handle = entt::null;
};