// Copyright (c) 2019-2020 Andrew Depke

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
	void* Handle = nullptr;
	bool CursorShown = true;
	CursorRestraint ActiveCursorRestraint = CursorRestraint::None;
	std::pair<int, int> CursorLockPosition;

public:
	// #TODO: Use delegates instead of std::function.
	std::function<void(bool)> OnFocusChanged;
	std::function<void(uint32_t, uint32_t, bool)> OnSizeChanged;

public:
	WindowFrame(const std::wstring& Title, uint32_t Width, uint32_t Height);
	~WindowFrame();

	void SetTitle(std::wstring Title);
	void SetSize(uint32_t Width, uint32_t Height);
	void ShowCursor(bool Visible);
	void RestrainCursor(CursorRestraint Restraint);

	void UpdateCursor();

	void* GetHandle() noexcept
	{
		return Handle;
	}
};