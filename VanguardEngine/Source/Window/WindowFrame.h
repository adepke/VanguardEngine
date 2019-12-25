// Copyright (c) 2019 Andrew Depke

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
	bool CursorRestrained = false;

public:
	WindowFrame(const std::wstring& Title, size_t Width, size_t Height, std::function<void(bool)>&& FocusChanged);
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