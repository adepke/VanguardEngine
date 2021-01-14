// Copyright (c) 2019-2021 Andrew Depke

#include <Window/WindowFrame.h>
#include <Core/Input.h>

#include <imgui.h>

#include <Core/Windows/WindowsMinimal.h>

constexpr auto windowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_VISIBLE | CS_HREDRAW | CS_VREDRAW;
constexpr auto windowStyleEx = 0;
constexpr auto windowClassStyle = CS_CLASSDC;
constexpr auto windowClassName = VGText("VanguardEngine");

RECT CreateCenteredRect(uint32_t width, uint32_t height)
{
	RECT result{};
	result.left = (::GetSystemMetrics(SM_CXSCREEN) / 2) - (static_cast<int>(width) / 2);
	result.top = (::GetSystemMetrics(SM_CYSCREEN) / 2) - (static_cast<int>(height) / 2);
	result.right = result.left + static_cast<int>(width);
	result.bottom = result.top + static_cast<int>(height);
	::AdjustWindowRect(&result, windowStyle, false);

	return result;
}

int64_t __stdcall WndProc(void* hWnd, uint32_t msg, uint64_t wParam, int64_t lParam)
{
	VGScopedCPUStat("Window Message Pump");

	auto* owningFrame = (WindowFrame*)::GetWindowLongPtr(static_cast<HWND>(hWnd), 0);  // Compiler won't allow a static cast here.
	if (!owningFrame)
	{
		return ::DefWindowProc(static_cast<HWND>(hWnd), msg, wParam, lParam);
	}

	switch (msg)
	{
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;

	case WM_MOVE:
		// #TODO: Not working.
		owningFrame->RestrainCursor(owningFrame->activeCursorRestraint);

		return ::DefWindowProc(static_cast<HWND>(hWnd), msg, wParam, lParam);

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED && owningFrame->onSizeChanged)
		{
			// #TODO: Handle fullscreen.
			owningFrame->onSizeChanged(static_cast<uint32_t>(LOWORD(lParam)), static_cast<uint32_t>(HIWORD(lParam)), false);
		}

		return 0;

	case WM_ACTIVATE:
		const auto active = LOWORD(wParam);
		if (active == WA_ACTIVE || active == WA_CLICKACTIVE)
		{
			if (owningFrame->onFocusChanged)
			{
				owningFrame->onFocusChanged(true);
			}

			owningFrame->RestrainCursor(owningFrame->activeCursorRestraint);
		}

		else
		{
			if (owningFrame->onFocusChanged)
			{
				owningFrame->onFocusChanged(false);
			}
		}

		return 0;
	}

	return Input::ProcessWindowMessage(hWnd, msg, wParam, lParam) ? 0 : ::DefWindowProc(static_cast<HWND>(hWnd), msg, wParam, lParam);
}

WindowFrame::WindowFrame(const std::wstring& title, uint32_t width, uint32_t height)
{
	VGScopedCPUStat("Create Window");

	const auto moduleHandle = ::GetModuleHandle(nullptr);

	auto windowRect{ CreateCenteredRect(width, height) };

	WNDCLASSEX windowDesc{};
	windowDesc.cbSize = sizeof(windowDesc);
	windowDesc.style = windowClassStyle;
	windowDesc.lpfnWndProc = (WNDPROC)&WndProc;
	windowDesc.cbClsExtra = 0;
	windowDesc.cbWndExtra = sizeof(this);  // Each instance stores the owning class pointer.
	windowDesc.hInstance = moduleHandle;
	windowDesc.hIcon = nullptr;
	windowDesc.hCursor = nullptr;
	windowDesc.hbrBackground = nullptr;
	windowDesc.lpszMenuName = 0;
	windowDesc.lpszClassName = windowClassName;
	windowDesc.hIconSm = nullptr;

	::RegisterClassEx(&windowDesc);

	handle = ::CreateWindowEx(
		windowStyleEx,
		windowDesc.lpszClassName,
		title.c_str(),
		windowStyle,
		windowRect.left,
		windowRect.top,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		windowDesc.hInstance,
		nullptr
	);

	if (!handle)
	{
		VGLogFatal(Window) << "Failed to create window: " << GetPlatformError();
	}

	// Save this class instance in the per-window memory.
	::SetWindowLongPtr(static_cast<HWND>(handle), 0, (LONG_PTR)this);  // Compiler won't allow a static cast here.

	// We need to resend the initial WM_SIZE message since the first one arrives before our per-instance memory is set (thus getting ignored).
	// Without this, the UI scaling is off until the next WM_SIZE message.

	RECT clientRect;
	::GetClientRect(static_cast<HWND>(handle), &clientRect);
	const auto clientWidth = clientRect.right - clientRect.left;
	const auto clientHeight = clientRect.bottom - clientRect.top;

	::PostMessage(static_cast<HWND>(handle), WM_SIZE, SIZE_RESTORED, (clientWidth & 0xFFFF) | ((clientHeight & 0xFFFF) << 16));
}

WindowFrame::~WindowFrame()
{
	VGScopedCPUStat("Destroy Window");

	::UnregisterClass(windowClassName, ::GetModuleHandle(nullptr));
}

void WindowFrame::SetTitle(std::wstring title)
{
	VGScopedCPUStat("Set Window Title");

	const auto result = ::SetWindowText(static_cast<HWND>(handle), title.c_str());
	if (!result)
	{
		VGLogError(Window) << "Failed to set title to: '" << title << "': " << GetPlatformError();
	}
}

void WindowFrame::SetSize(uint32_t width, uint32_t height)
{
	VGScopedCPUStat("Set Window Size");

	const auto rect{ CreateCenteredRect(width, height) };

	const auto result = ::SetWindowPos(
		static_cast<HWND>(handle),
		HWND_NOTOPMOST,
		rect.left,
		rect.top,
		rect.right - rect.left,
		rect.bottom - rect.top,
		0);  // Possibly SWP_NOREPOSITION

	if (!result)
	{
		VGLogError(Window) << "Failed to set size to: (" << width << ", " << height << "): " << GetPlatformError();
	}

	// Window size updated, we need to update the clipping bounds.
	RestrainCursor(activeCursorRestraint);
}

void WindowFrame::ShowCursor(bool visible)
{
	VGScopedCPUStat("Show Window Cursor");

	// ShowCursor acts like a stack, but we don't want this kind of behavior.
	if ((visible && !cursorShown) || (!visible && cursorShown))
	{
		cursorShown = visible;
		::ShowCursor(visible);
	}
}

void WindowFrame::RestrainCursor(CursorRestraint restraint)
{
	VGScopedCPUStat("Restrain Window Cursor");

	activeCursorRestraint = restraint;

	switch (restraint)
	{
	case CursorRestraint::None:
	{
		const auto result = ::ClipCursor(nullptr);
		if (!result)
		{
			VGLogError(Window) << "Failed to unrestrain cursor: " << GetPlatformError();
		}

		break;
	}

	case CursorRestraint::ToCenter:
	{
		// Disable any clipping first.
		const auto result = ::ClipCursor(nullptr);
		if (!result)
		{
			VGLogError(Window) << "Failed to unrestrain cursor: " << GetPlatformError();
		}

		RECT clientRect;
		::GetClientRect(static_cast<HWND>(handle), &clientRect);

		POINT topLeft = { clientRect.left, clientRect.top };
		POINT bottomRight = { clientRect.right, clientRect.bottom };

		// Convert the local space coordinates to screen space.
		::ClientToScreen(static_cast<HWND>(handle), &topLeft);
		::ClientToScreen(static_cast<HWND>(handle), &bottomRight);

		cursorLockPosition = std::make_pair(static_cast<int>(topLeft.x + (bottomRight.x - topLeft.x) * 0.5f), static_cast<int>(topLeft.y + (bottomRight.y - topLeft.y) * 0.5f));

		break;
	}

	case CursorRestraint::ToWindow:
	{
		RECT clientRect;
		::GetClientRect(static_cast<HWND>(handle), &clientRect);

		POINT topLeft = { clientRect.left, clientRect.top };
		POINT bottomRight = { clientRect.right, clientRect.bottom };

		// Convert the local space coordinates to screen space.
		::ClientToScreen(static_cast<HWND>(handle), &topLeft);
		::ClientToScreen(static_cast<HWND>(handle), &bottomRight);

		clientRect.left = topLeft.x;
		clientRect.top = topLeft.y;
		clientRect.right = bottomRight.x;
		clientRect.bottom = bottomRight.y;

		const auto result = ::ClipCursor(&clientRect);
		if (!result)
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
	if (activeCursorRestraint == CursorRestraint::ToCenter && ::GetFocus() == static_cast<HWND>(GetHandle()))
	{
		if (!::SetCursorPos(cursorLockPosition.first, cursorLockPosition.second))
		{
			VGLogWarning(Window) << "Failed to set cursor position to window center: " << GetPlatformError();
		}

		else
		{
			POINT clientMousePos{ cursorLockPosition.first, cursorLockPosition.second };

			// ImGui stores cursor positions in client space.
			if (!::ScreenToClient(static_cast<HWND>(GetHandle()), &clientMousePos))
			{
				VGLogWarning(Window) << "Failed to convert mouse position from screen space to window space: " << GetPlatformError();
			}

			else
			{
				// Set the previous position to the center so that next frame's delta doesn't treat this cursor update as a normal mouse move.
				ImGui::GetIO().MousePosPrev = { static_cast<float>(clientMousePos.x), static_cast<float>(clientMousePos.y) };
			}
		}
	}
}