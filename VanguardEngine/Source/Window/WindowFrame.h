// Copyright (c) 2019-2021 Andrew Depke

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

public:
	// #TODO: Use delegates instead of std::function.
	std::function<void(bool)> onFocusChanged;
	std::function<void(uint32_t, uint32_t, bool)> onSizeChanged;

public:
	WindowFrame(const std::wstring& title, uint32_t width, uint32_t height);
	~WindowFrame();

	void SetTitle(std::wstring title);
	void SetSize(uint32_t width, uint32_t height);
	void ShowCursor(bool visible);
	void RestrainCursor(CursorRestraint restraint);

	void UpdateCursor();

	void* GetHandle() noexcept
	{
		return handle;
	}
};