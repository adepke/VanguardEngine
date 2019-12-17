// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Utility/FunctionRef.h>

class WindowFrame
{
private:
	void* Handle;
	FunctionRef<void(bool)> OnFocusChanged;

public:
	WindowFrame(const std::wstring& Title, size_t Width, size_t Height, FunctionRef<void(bool)>&& FocusChanged);
	~WindowFrame();

	void SetTitle(std::wstring Title);
	void SetSize(size_t Width, size_t Height);
	void ShowCursor(bool Visible);

	void* GetHandle() noexcept
	{
		return Handle;
	}
};