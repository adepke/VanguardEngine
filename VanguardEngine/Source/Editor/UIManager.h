// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Rendering/Viewport.h>

#include <entt/entt.hpp>  // #TODO: Don't include this here.

class UIManager
{
public:
	Viewport SceneViewport;

	void DrawScene();
	void DrawEntityViewer(entt::registry& Registry);

public:
	static inline UIManager& Get() noexcept
	{
		static UIManager Singleton;
		return Singleton;
	}

	UIManager() = default;
	UIManager(const UIManager&) = delete;
	UIManager(UIManager&&) noexcept = delete;

	UIManager& operator=(const UIManager&) = delete;
	UIManager& operator=(UIManager&&) noexcept = delete;
};