// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>

#include <entt/entt.hpp>  // #TODO: Don't include this here.

class UIManager
{
private:
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

	// Single interface to draw the editor from the renderer.
	void Render(entt::registry& Registry);
};