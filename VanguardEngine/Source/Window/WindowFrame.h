// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Core/Base.h>

#include <functional>
#include <utility>

enum class CursorRestraint
{
	None,
	ToCenter,
	ToWindow,
};

int64_t __stdcall WndProc(void* hWnd, uint32_t msg, uint64_t wParam, int64_t lParam);

class WindowFrame
{
	friend int64_t WndProc(void*, uint32_t, uint64_t, int64_t);

private:
	void* handle = nullptr;
	bool cursorShown = true;
	CursorRestraint activeCursorRestraint = CursorRestraint::None;
	std::pair<int, int> cursorLockPosition;
	uint32_t width;
	uint32_t height;
	bool fullscreen = false;

	// Used to return to original window size when leaving fullscreen.
	uint32_t oldWidth;
	uint32_t oldHeight;

public:
	// #TODO: Use delegates instead of std::function.
	std::function<void(bool)> onFocusChanged;
	std::function<void(uint32_t, uint32_t)> onSizeChanged;

public:
	WindowFrame(const std::wstring& title, uint32_t inWidth, uint32_t inHeight);
	~WindowFrame();

	void SetTitle(const std::wstring& title);
	void SetSize(uint32_t inWidth, uint32_t inHeight, bool inFullscreen);
	void ShowCursor(bool visible);
	void RestrainCursor(CursorRestraint restraint);

	void UpdateCursor();

	void* GetHandle() noexcept
	{
		return handle;
	}

	bool IsFullscreen() const noexcept
	{
		return fullscreen;
	}
};