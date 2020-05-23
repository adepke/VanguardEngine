// Copyright (c) 2019-2020 Andrew Depke

#include <Window/WindowFrame.h>
#include <Core/Windows/WindowsMinimal.h>
#include <Core/InputManager.h>

#include <imgui.h>

constexpr auto WindowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_VISIBLE | CS_HREDRAW | CS_VREDRAW;
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
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;

	case WM_MOVE:
		// #TODO: Not working.
		WindowFrame::Get().RestrainCursor(WindowFrame::Get().ActiveCursorRestraint);

		return ::DefWindowProc(hWnd, msg, wParam, lParam);

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED && WindowFrame::Get().OnSizeChanged)
		{
			// #TODO: Handle fullscreen.
			WindowFrame::Get().OnSizeChanged(static_cast<size_t>(LOWORD(lParam)), static_cast<size_t>(HIWORD(lParam)), false);
		}

		return 0;

	case WM_DPICHANGED:
		// #TODO: DPI awareness.
		return ::DefWindowProc(hWnd, msg, wParam, lParam);

	case WM_ACTIVATE:
		const auto Active = LOWORD(wParam);
		if (Active == WA_ACTIVE || Active == WA_CLICKACTIVE)
		{
			if (WindowFrame::Get().OnFocusChanged)
			{
				WindowFrame::Get().OnFocusChanged(true);
			}

			WindowFrame::Get().RestrainCursor(WindowFrame::Get().ActiveCursorRestraint);
		}

		else
		{
			if (WindowFrame::Get().OnFocusChanged)
			{
				WindowFrame::Get().OnFocusChanged(false);
			}
		}

		return 0;
	}

	return InputManager::Get().ProcessWindowMessage(msg, wParam, lParam) ? 0 : ::DefWindowProc(hWnd, msg, wParam, lParam);
}

WindowFrame::~WindowFrame()
{
	VGScopedCPUStat("Destroy Window");

	::UnregisterClass(WindowClassName, ::GetModuleHandle(nullptr));
}

void WindowFrame::Create(const std::wstring& Title, size_t Width, size_t Height)
{
	VGScopedCPUStat("Create Window");

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
	RestrainCursor(ActiveCursorRestraint);
}

void WindowFrame::ShowCursor(bool Visible)
{
	VGScopedCPUStat("Show Window Cursor");

	// ShowCursor acts like a stack, but we don't want this kind of behavior.
	if ((Visible && !CursorShown) || (!Visible && CursorShown))
	{
		CursorShown = Visible;
		::ShowCursor(Visible);
	}
}

void WindowFrame::RestrainCursor(CursorRestraint Restraint)
{
	VGScopedCPUStat("Restrain Window Cursor");

	ActiveCursorRestraint = Restraint;

	switch (Restraint)
	{
	case CursorRestraint::None:
	{
		const auto Result = ::ClipCursor(nullptr);
		if (!Result)
		{
			VGLogError(Window) << "Failed to unrestrain cursor: " << GetPlatformError();
		}

		break;
	}

	case CursorRestraint::ToCenter:
	{
		// Disable any clipping first.
		const auto Result = ::ClipCursor(nullptr);
		if (!Result)
		{
			VGLogError(Window) << "Failed to unrestrain cursor: " << GetPlatformError();
		}

		RECT ClientRect;
		::GetClientRect(static_cast<HWND>(Handle), &ClientRect);

		POINT TopLeft = { ClientRect.left, ClientRect.top };
		POINT BottomRight = { ClientRect.right, ClientRect.bottom };

		// Convert the local space coordinates to screen space.
		::ClientToScreen(static_cast<HWND>(Handle), &TopLeft);
		::ClientToScreen(static_cast<HWND>(Handle), &BottomRight);

		CursorLockPosition = std::make_pair(TopLeft.x + (BottomRight.x - TopLeft.x) * 0.5f, TopLeft.y + (BottomRight.y - TopLeft.y) * 0.5f);

		break;
	}

	case CursorRestraint::ToWindow:
	{
		RECT ClientRect;
		::GetClientRect(static_cast<HWND>(Handle), &ClientRect);

		POINT TopLeft = { ClientRect.left, ClientRect.top };
		POINT BottomRight = { ClientRect.right, ClientRect.bottom };

		// Convert the local space coordinates to screen space.
		::ClientToScreen(static_cast<HWND>(Handle), &TopLeft);
		::ClientToScreen(static_cast<HWND>(Handle), &BottomRight);

		ClientRect.left = TopLeft.x;
		ClientRect.top = TopLeft.y;
		ClientRect.right = BottomRight.x;
		ClientRect.bottom = BottomRight.y;

		const auto Result = ::ClipCursor(&ClientRect);
		if (!Result)
		{
			VGLogError(Window) << "Failed to restrain cursor: " << GetPlatformError();
		}

		break;
	}
	}
}

void WindowFrame::UpdateCursor()
{
	// Apply centering restraint if that's active.
	if (ActiveCursorRestraint == CursorRestraint::ToCenter && ::GetFocus() == static_cast<HWND>(GetHandle()))
	{
		if (!::SetCursorPos(CursorLockPosition.first, CursorLockPosition.second))
		{
			VGLogWarning(Window) << "Failed to set cursor position to window center: " << GetPlatformError();
		}

		else
		{
			POINT ClientMousePos{ CursorLockPosition.first, CursorLockPosition.second };

			// ImGui stores cursor positions in client space.
			if (!::ScreenToClient(static_cast<HWND>(GetHandle()), &ClientMousePos))
			{
				VGLogWarning(Window) << "Failed to convert mouse position from screen space to window space: " << GetPlatformError();
			}

			else
			{
				// Set the previous position to the center so that next frame's delta doesn't treat this cursor update as a normal mouse move.
				ImGui::GetIO().MousePosPrev = { static_cast<float>(ClientMousePos.x), static_cast<float>(ClientMousePos.y) };
			}
		}
	}
}