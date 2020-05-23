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

class WindowFrame
{
private:
	void* Handle = nullptr;
	bool CursorShown = true;

// #NOTE: We need to make a bunch of these variables public in order to be accessible from WndProc. We also can't friend WndProc due to win32 API requirements.
public:
	std::function<void(bool)> OnFocusChanged;
	std::function<void(size_t, size_t, bool)> OnSizeChanged;
	CursorRestraint ActiveCursorRestraint = CursorRestraint::None;
	std::pair<int, int> CursorLockPosition;

public:
	static inline WindowFrame& Get() noexcept
	{
		static WindowFrame Singleton;
		return Singleton;
	}

	WindowFrame() = default;
	WindowFrame(const WindowFrame&) = delete;
	WindowFrame(WindowFrame&&) noexcept = delete;

	WindowFrame& operator=(const WindowFrame&) = delete;
	WindowFrame& operator=(WindowFrame&&) noexcept = delete;
	
	~WindowFrame();

	void Create(const std::wstring& Title, size_t Width, size_t Height);

	void SetTitle(std::wstring Title);
	void SetSize(size_t Width, size_t Height);
	void ShowCursor(bool Visible);
	void RestrainCursor(CursorRestraint Restraint);

	void UpdateCursor();

	void* GetHandle() noexcept
	{
		return Handle;
	}
};