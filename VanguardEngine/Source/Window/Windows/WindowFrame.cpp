// Copyright (c) 2019 Andrew Depke

#include <Window/WindowFrame.h>
#include <Core/Windows/WindowsMinimal.h>

WindowFrame* GlobalWindow;  // #NOTE: This is assuming we're only ever going to have one active WindowFrame at a time.

constexpr auto WindowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_VISIBLE;
constexpr auto WindowStyleEx = 0;
constexpr auto WindowClassStyle = CS_CLASSDC;
constexpr auto WindowClassName = VGText("VanguardEngine");

RECT CreateCenteredRect(size_t Width, size_t Height)
{
	RECT Result{};
	Result.left = (::GetSystemMetrics(SM_CXSCREEN) / 2) - (static_cast<int>(Width) / 2);
	Result.top = (::GetSystemMetrics(SM_CYSCREEN) / 2) - (static_cast<int>(Height) / 2);
	Result.right = Result.left + static_cast<int>(Width);
	Result.bottom = Result.top + static_cast<int>(Height);
	::AdjustWindowRect(&Result, WindowStyle, false);

	return Result;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	VGScopedCPUStat("Window Message Pump");

	switch (msg)
	{
	case WM_QUIT:
		::PostQuitMessage(0);
		return 0;

	case WM_MOVE:
		// #TODO: Not working.
		if (GlobalWindow->CursorRestrained)
		{
			GlobalWindow->RestrainCursor(true);
		}

		return 0;

	case WM_DPICHANGED:
		// #TODO: DPI awareness.
		return ::DefWindowProc(hWnd, msg, wParam, lParam);

	case WM_ACTIVATE:
		const auto Active = LOWORD(wParam);
		if (Active == WA_ACTIVE || Active == WA_CLICKACTIVE)
		{
			GlobalWindow->OnFocusChanged(true);
			if (GlobalWindow->CursorRestrained)
			{
				GlobalWindow->RestrainCursor(true);
			}
		}

		else
		{
			GlobalWindow->OnFocusChanged(false);
		}

		return 0;
	}

	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

WindowFrame::WindowFrame(const std::wstring& Title, size_t Width, size_t Height, std::function<void(bool)>&& FocusChanged) : OnFocusChanged(std::move(FocusChanged))
{
	VGScopedCPUStat("Create Window");

	GlobalWindow = this;

	const auto ModuleHandle = ::GetModuleHandle(nullptr);

	auto WindowRect{ CreateCenteredRect(Width, Height) };

	WNDCLASSEX WindowDesc{};
	WindowDesc.cbSize = sizeof(WindowDesc);
	WindowDesc.style = WindowClassStyle;
	WindowDesc.lpfnWndProc = &WndProc;
	WindowDesc.cbClsExtra = 0;
	WindowDesc.cbWndExtra = 0;
	WindowDesc.hInstance = ModuleHandle;
	WindowDesc.hIcon = nullptr;
	WindowDesc.hCursor = nullptr;
	WindowDesc.hbrBackground = nullptr;
	WindowDesc.lpszMenuName = 0;
	WindowDesc.lpszClassName = WindowClassName;
	WindowDesc.hIconSm = nullptr;

	::RegisterClassEx(&WindowDesc);

	Handle = ::CreateWindowEx(
		WindowStyleEx,
		WindowDesc.lpszClassName,
		Title.c_str(),
		WindowStyle,
		WindowRect.left,
		WindowRect.top,
		WindowRect.right - WindowRect.left,
		WindowRect.bottom - WindowRect.top,
		nullptr,
		nullptr,
		WindowDesc.hInstance,
		nullptr
	);

	if (!Handle)
	{
		VGLogFatal(Window) << "Failed to create window: " << GetPlatformError();
	}
}

WindowFrame::~WindowFrame()
{
	VGScopedCPUStat("Destroy Window");

	::UnregisterClass(WindowClassName, ::GetModuleHandle(nullptr));
}

void WindowFrame::SetTitle(std::wstring Title)
{
	VGScopedCPUStat("Set Window Title");

	const auto Result = ::SetWindowText(static_cast<HWND>(Handle), Title.c_str());
	if (!Result)
	{
		VGLogError(Window) << "Failed to set title to: '" << Title << "': " << GetPlatformError();
	}
}

void WindowFrame::SetSize(size_t Width, size_t Height)
{
	VGScopedCPUStat("Set Window Size");

	const auto Rect{ CreateCenteredRect(Width, Height) };

	const auto Result = ::SetWindowPos(
		static_cast<HWND>(Handle),
		HWND_NOTOPMOST,
		Rect.left,
		Rect.top,
		Rect.right - Rect.left,
		Rect.bottom - Rect.top,
		0);  // Possibly SWP_NOREPOSITION

	if (!Result)
	{
		VGLogError(Window) << "Failed to set size to: (" << Width << ", " << Height << "): " << GetPlatformError();
	}

	// Window size updated, we need to update the clipping bounds.
	if (CursorRestrained)
	{
		RestrainCursor(true);
	}
}

void WindowFrame::ShowCursor(bool Visible)
{
	VGScopedCPUStat("Show Window Cursor");

	::ShowCursor(Visible);
}

void WindowFrame::RestrainCursor(bool Restrain)
{
	VGScopedCPUStat("Restrain Window Cursor");

	CursorRestrained = Restrain;

	if (Restrain)
	{
		RECT WindowRect;
		::GetWindowRect(static_cast<HWND>(Handle), &WindowRect);

		// #TODO: Minimize rect to actually keep the cursor entirely in the drawable interface of the window.

		const auto Result = ::ClipCursor(&WindowRect);
		if (!Result)
		{
			VGLogError(Window) << "Failed to restrain cursor: " << GetPlatformError();
		}
	}

	else
	{
		const auto Result = ::ClipCursor(nullptr);
		if (!Result)
		{
			VGLogError(Window) << "Failed to restrain cursor: " << GetPlatformError();
		}
	}
}