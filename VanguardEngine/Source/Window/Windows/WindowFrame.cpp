// Copyright (c) 2019-2020 Andrew Depke

#include <Window/WindowFrame.h>
#include <Core/Input.h>

#include <imgui.h>

#include <Core/Windows/WindowsMinimal.h>

constexpr auto WindowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX | WS_VISIBLE | CS_HREDRAW | CS_VREDRAW;
constexpr auto WindowStyleEx = 0;
constexpr auto WindowClassStyle = CS_CLASSDC;
constexpr auto WindowClassName = VGText("VanguardEngine");

RECT CreateCenteredRect(uint32_t Width, uint32_t Height)
{
	RECT Result{};
	Result.left = (::GetSystemMetrics(SM_CXSCREEN) / 2) - (static_cast<int>(Width) / 2);
	Result.top = (::GetSystemMetrics(SM_CYSCREEN) / 2) - (static_cast<int>(Height) / 2);
	Result.right = Result.left + static_cast<int>(Width);
	Result.bottom = Result.top + static_cast<int>(Height);
	::AdjustWindowRect(&Result, WindowStyle, false);

	return Result;
}

int64_t __stdcall WndProc(void* hWnd, uint32_t msg, uint64_t wParam, int64_t lParam)
{
	VGScopedCPUStat("Window Message Pump");

	auto* OwningFrame = (WindowFrame*)::GetWindowLongPtr(static_cast<HWND>(hWnd), 0);  // Compiler won't allow a static cast here.
	if (!OwningFrame)
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
		OwningFrame->RestrainCursor(OwningFrame->ActiveCursorRestraint);

		return ::DefWindowProc(static_cast<HWND>(hWnd), msg, wParam, lParam);

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED && OwningFrame->OnSizeChanged)
		{
			// #TODO: Handle fullscreen.
			OwningFrame->OnSizeChanged(static_cast<uint32_t>(LOWORD(lParam)), static_cast<uint32_t>(HIWORD(lParam)), false);
		}

		return 0;

	case WM_ACTIVATE:
		const auto Active = LOWORD(wParam);
		if (Active == WA_ACTIVE || Active == WA_CLICKACTIVE)
		{
			if (OwningFrame->OnFocusChanged)
			{
				OwningFrame->OnFocusChanged(true);
			}

			OwningFrame->RestrainCursor(OwningFrame->ActiveCursorRestraint);
		}

		else
		{
			if (OwningFrame->OnFocusChanged)
			{
				OwningFrame->OnFocusChanged(false);
			}
		}

		return 0;
	}

	return Input::ProcessWindowMessage(hWnd, msg, wParam, lParam) ? 0 : ::DefWindowProc(static_cast<HWND>(hWnd), msg, wParam, lParam);
}

WindowFrame::WindowFrame(const std::wstring& Title, uint32_t Width, uint32_t Height)
{
	VGScopedCPUStat("Create Window");

	const auto ModuleHandle = ::GetModuleHandle(nullptr);

	auto WindowRect{ CreateCenteredRect(Width, Height) };

	WNDCLASSEX WindowDesc{};
	WindowDesc.cbSize = sizeof(WindowDesc);
	WindowDesc.style = WindowClassStyle;
	WindowDesc.lpfnWndProc = (WNDPROC)&WndProc;
	WindowDesc.cbClsExtra = 0;
	WindowDesc.cbWndExtra = sizeof(this);  // Each instance stores the owning class pointer.
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

	// Save this class instance in the per-window memory.
	::SetWindowLongPtr(static_cast<HWND>(Handle), 0, (LONG_PTR)this);  // Compiler won't allow a static cast here.
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

void WindowFrame::SetSize(uint32_t Width, uint32_t Height)
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

		CursorLockPosition = std::make_pair(static_cast<int>(TopLeft.x + (BottomRight.x - TopLeft.x) * 0.5f), static_cast<int>(TopLeft.y + (BottomRight.y - TopLeft.y) * 0.5f));

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