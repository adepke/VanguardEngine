// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>

#include <functional>

// #NOTE: Behaves as a singleton, do not create more than one at any given time.
class WindowFrame
{
private:
	void* Handle;

// #NOTE: We need to make a bunch of these variables public in order to be accessible from WndProc.
public:
	std::function<void(bool)> OnFocusChanged;
	std::function<void(size_t, size_t, bool)> OnSizeChanged;
	bool CursorRestrained = false;

public:
	WindowFrame(const std::wstring& Title, size_t Width, size_t Height);
	~WindowFrame();

	void SetTitle(std::wstring Title);
	void SetSize(size_t Width, size_t Height);
	void ShowCursor(bool Visible);
	void RestrainCursor(bool Restrain);

	void* GetHandle() noexcept
	{
		return Handle;
	}
};