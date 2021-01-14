// Copyright (c) 2019-2021 Andrew Depke

#include <Core/Input.h>
#include <Window/WindowFrame.h>

#include <imgui.h>

#include <limits>

#include <Core/Windows/WindowsMinimal.h>
#include <XInput.h>  // Required for gamepad support.
#include <ShellScalingApi.h>

namespace Input
{
	static bool pendingMonitorUpdate = true;

	float GetDPIScale(void* monitor)
	{
		uint32_t dpiX = 96;
		uint32_t dpiY = 96;

		if (FAILED(::GetDpiForMonitor(static_cast<HMONITOR>(monitor), MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
		{
			VGLogError(Core) << "Failed to get monitor DPI.";

			return 1.f;
		}

		return dpiX / 96.f;
	}

	void UpdateMonitors()
	{
		VGScopedCPUStat("Update Monitors");

		pendingMonitorUpdate = false;

		ImGui::GetPlatformIO().Monitors.resize(0);

		::EnumDisplayMonitors(nullptr, nullptr, MONITORENUMPROC(+[](HMONITOR monitor, HDC, LPRECT, LPARAM)
			{
				MONITORINFO monitorInfo{};
				monitorInfo.cbSize = sizeof(monitorInfo);

				if (!::GetMonitorInfo(monitor, &monitorInfo))
				{
					return true;
				}

				ImGuiPlatformMonitor platformMonitor;
				platformMonitor.MainPos = { static_cast<float>(monitorInfo.rcMonitor.left), static_cast<float>(monitorInfo.rcMonitor.top) };
				platformMonitor.MainSize = { static_cast<float>(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left), static_cast<float>(monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top) };
				platformMonitor.WorkPos = { static_cast<float>(monitorInfo.rcWork.left), static_cast<float>(monitorInfo.rcWork.top) };
				platformMonitor.WorkSize = { static_cast<float>(monitorInfo.rcWork.right - monitorInfo.rcWork.left), static_cast<float>(monitorInfo.rcWork.bottom - monitorInfo.rcWork.top) };
				platformMonitor.DpiScale = GetDPIScale(monitor);

				auto& platformIO = ImGui::GetPlatformIO();

				if (monitorInfo.dwFlags & MONITORINFOF_PRIMARY)
				{
					platformIO.Monitors.push_front(platformMonitor);
				}

				else
				{
					platformIO.Monitors.push_back(platformMonitor);
				}

				return true;
			}), 0);
	}

	// #TODO: Implement viewports features.

	void UpdateKeyboard()
	{
		VGScopedCPUStat("Update Keyboard");

		auto& io = ImGui::GetIO();

		// Update key modifiers that aren't handled by the input processing.
		io.KeyCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
		io.KeyShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
		io.KeyAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
		io.KeySuper = false;
	}

	void UpdateMouse(void* window)
	{
		VGScopedCPUStat("Update Mouse");

		auto& io = ImGui::GetIO();

		if (io.WantSetMousePos)
		{
			POINT targetPoint = { static_cast<int>(io.MousePos.x), static_cast<int>(io.MousePos.y) };

			::SetCursorPos(targetPoint.x, targetPoint.y);
		}

		io.MousePos.x = std::numeric_limits<float>::min();
		io.MousePos.y = std::numeric_limits<float>::min();
		io.MouseHoveredViewport = 0;

		POINT mousePosition;
		if (!::GetCursorPos(&mousePosition))
		{
			VGLogWarning(Core) << "Failed to get mouse cursor position: " << GetPlatformError();

			return;
		}

		if (auto* foregroundWindow = ::GetForegroundWindow(); foregroundWindow)
		{
			if (::IsChild(foregroundWindow, static_cast<HWND>(window)))
			{
				foregroundWindow = static_cast<HWND>(window);
			}

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				if (ImGui::FindViewportByPlatformHandle(foregroundWindow))
				{
					io.MousePos = { static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y) };
				}
			}

			else
			{
				if (foregroundWindow == window)
				{
					if (!::ScreenToClient(static_cast<HWND>(window), &mousePosition))
						VGLogWarning(Core) << "Failed to convert mouse position from screen space to window space: " << GetPlatformError();
					else
						io.MousePos = { static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y) };
				}
			}
		}

		if (auto hoveredWindow = ::WindowFromPoint(mousePosition); hoveredWindow)
		{
			if (auto* viewport = ImGui::FindViewportByPlatformHandle(hoveredWindow); viewport)
			{
				if (!(viewport->Flags & ImGuiViewportFlags_NoInputs))
				{
					io.MouseHoveredViewport = viewport->ID;
				}
			}
		}

		auto cursor = ImGui::GetMouseCursor();

		if (cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
		{
			::SetCursor(nullptr);  // Hide the cursor.
		}

		else
		{
			// Default to arrow.
			LPTSTR platformCursor = IDC_ARROW;

			switch (cursor)
			{
			case ImGuiMouseCursor_Arrow: platformCursor = IDC_ARROW; break;
			case ImGuiMouseCursor_TextInput: platformCursor = IDC_IBEAM; break;
			case ImGuiMouseCursor_ResizeAll: platformCursor = IDC_SIZEALL; break;
			case ImGuiMouseCursor_ResizeEW: platformCursor = IDC_SIZEWE; break;
			case ImGuiMouseCursor_ResizeNS: platformCursor = IDC_SIZENS; break;
			case ImGuiMouseCursor_ResizeNESW: platformCursor = IDC_SIZENESW; break;
			case ImGuiMouseCursor_ResizeNWSE: platformCursor = IDC_SIZENWSE; break;
			case ImGuiMouseCursor_Hand: platformCursor = IDC_HAND; break;
			case ImGuiMouseCursor_NotAllowed: platformCursor = IDC_NO; break;
			}

			if (!::SetCursor(::LoadCursor(nullptr, platformCursor)))
			{
				VGLogWarning(Core) << "Failed to set cursor: " << GetPlatformError();
			}
		}
	}

	void UpdateGamepad()
	{
		VGScopedCPUStat("Update Gamepad");

		// #TODO: Implement support for gamepads.
	}

	void Initialize(void* window)
	{
		VGScopedCPUStat("Input Initialize");

		// Ensure we have an ImGui context.
		if (!ImGui::GetCurrentContext())
		{
			VGLogFatal(Core) << "Missing ImGui context!";
		}

		auto& io = ImGui::GetIO();

		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
		io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
		io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
		io.BackendPlatformName = "Vanguard Win64";

		ImGui::GetMainViewport()->PlatformHandleRaw = window;

		io.KeyMap[ImGuiKey_Tab] = VK_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
		io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
		io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
		io.KeyMap[ImGuiKey_Home] = VK_HOME;
		io.KeyMap[ImGuiKey_End] = VK_END;
		io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
		io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
		io.KeyMap[ImGuiKey_Space] = VK_SPACE;
		io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
		io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
		io.KeyMap[ImGuiKey_KeyPadEnter] = VK_RETURN;
		io.KeyMap[ImGuiKey_A] = 'A';
		io.KeyMap[ImGuiKey_C] = 'C';
		io.KeyMap[ImGuiKey_V] = 'V';
		io.KeyMap[ImGuiKey_X] = 'X';
		io.KeyMap[ImGuiKey_Y] = 'Y';
		io.KeyMap[ImGuiKey_Z] = 'Z';
	}

	void EnableDPIAwareness()
	{
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	}

	bool ProcessWindowMessage(void* window, uint32_t message, int64_t wParam, uint64_t lParam)
	{
		VGScopedCPUStat("Process Input Messages");

		// Ensure we have an ImGui context.
		if (!ImGui::GetCurrentContext())
			return false;

		auto& io = ImGui::GetIO();

		switch (message)
		{
		// Mouse click events.

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
		{
			size_t mouseButton = 0;  // Default to left click.

			if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) mouseButton = 1;
			if (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK) mouseButton = 2;
			if (message == WM_XBUTTONDOWN || message == WM_XBUTTONDBLCLK) mouseButton = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4;

			if (!ImGui::IsAnyMouseDown() && !::GetCapture())
			{
				::SetCapture(static_cast<HWND>(window));
			}

			io.MouseDown[mouseButton] = true;

			return true;
		}

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
		{
			size_t mouseButton = 0;  // Default to left click.

			if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) mouseButton = 1;
			if (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK) mouseButton = 2;
			if (message == WM_XBUTTONDOWN || message == WM_XBUTTONDBLCLK) mouseButton = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4;

			io.MouseDown[mouseButton] = false;

			if (!ImGui::IsAnyMouseDown() && ::GetCapture() == window)
			{
				::ReleaseCapture();
			}

			return true;
		}

		// Mouse scroll events.

		case WM_MOUSEWHEEL:
		{
			io.MouseWheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);

			return true;
		}

		case WM_MOUSEHWHEEL:
		{
			io.MouseWheelH += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);

			return true;
		}

		// Keyboard events.

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			if (wParam < 256)
			{
				io.KeysDown[wParam] = true;

				return true;
			}

			return false;  // We don't handle these keys.
		}

		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			if (wParam < 256)
			{
				io.KeysDown[wParam] = false;

				return true;
			}

			return false;  // We don't handle these keys.
		}

		case WM_CHAR:
		{
			if (wParam > 0 && wParam < 0x10000)
			{
				io.AddInputCharacterUTF16(static_cast<unsigned short>(wParam));

				return true;
			}

			return false;
		}

		// Display events.

		case WM_DISPLAYCHANGE:
		{
			pendingMonitorUpdate = true;

			return true;
		}
		}

		return false;
	}

	void UpdateInputDevices(void* window)
	{
		VGScopedCPUStat("Update Input Devices");

		// If we have a monitor update, run it before any mouse-related tasks.
		if (pendingMonitorUpdate)
		{
			UpdateMonitors();
		}

		UpdateKeyboard();
		UpdateMouse(window);
		UpdateGamepad();
	}
}