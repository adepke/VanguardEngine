// Copyright (c) 2019 Andrew Depke

#include <Window/WindowFrame.h>
#include <Core/Windows/WindowsMinimal.h>

constexpr auto WindowClassName = VGText("VanguardEngine");

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	VGScopedCPUStat("Window Frame Message Pump");

	switch (msg)
	{
	case WM_QUIT:
		PostQuitMessage(0);
		return 0;
	case WM_DPICHANGED:
		// #TODO: DPI awareness.
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

WindowFrame::WindowFrame(const std::wstring& Title, size_t Width, size_t Height, FunctionRef<void(bool)>&& FocusChanged) : OnFocusChanged(FocusChanged)
{
	VGScopedCPUStat("Build Window Frame");

	const auto ModuleHandle = GetModuleHandle(nullptr);

	constexpr auto WindowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_VISIBLE;
	constexpr auto WindowStyleEx = 0;
	constexpr auto WindowClassStyle = CS_CLASSDC;

	RECT WindowRect{};
	WindowRect.left = (GetSystemMetrics(SM_CXSCREEN) / 2) - (static_cast<int>(Width) / 2);
	WindowRect.top = (GetSystemMetrics(SM_CYSCREEN) / 2) - (static_cast<int>(Height) / 2);
	WindowRect.right = WindowRect.left + static_cast<int>(Width);
	WindowRect.bottom = WindowRect.top + static_cast<int>(Height);
	AdjustWindowRect(&WindowRect, WindowStyle, false);

	WNDCLASSEX WindowDesc{};
	ZeroMemory(&WindowDesc, sizeof(WindowDesc));
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

	RegisterClassEx(&WindowDesc);

	Handle = CreateWindowEx(
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

	VGEnsure(Handle, "Failed to create window.");
}

WindowFrame::~WindowFrame()
{
	VGScopedCPUStat("Destroy Window Frame");

	UnregisterClass(WindowClassName, GetModuleHandle(nullptr));
}

void WindowFrame::SetTitle(std::wstring Title)
{

}

void WindowFrame::SetSize(size_t Width, size_t Height)
{

}

void WindowFrame::ShowCursor(bool Visible)
{

}