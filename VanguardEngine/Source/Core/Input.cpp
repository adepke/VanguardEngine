// Copyright (c) 2019-2020 Andrew Depke

#include <Core/Input.h>
#include <Window/WindowFrame.h>

#include <imgui.h>

#include <limits>

#include <Core/Windows/WindowsMinimal.h>
#include <XInput.h>  // Required for gamepad support.
#include <ShellScalingApi.h>

namespace Input
{
	static bool PendingMonitorUpdate = true;

	float GetDPIScale(void* Monitor)
	{
		uint32_t DPIX = 96;
		uint32_t DPIY = 96;

		if (FAILED(::GetDpiForMonitor(static_cast<HMONITOR>(Monitor), MDT_EFFECTIVE_DPI, &DPIX, &DPIY)))
		{
			VGLogError(Core) << "Failed to get monitor DPI.";

			return 1.f;
		}

		return DPIX / 96.f;
	}

	void UpdateMonitors()
	{
		PendingMonitorUpdate = false;

		ImGui::GetPlatformIO().Monitors.resize(0);

		::EnumDisplayMonitors(nullptr, nullptr, MONITORENUMPROC(+[](HMONITOR Monitor, HDC, LPRECT, LPARAM)
			{
				MONITORINFO MonitorInfo{};
				MonitorInfo.cbSize = sizeof(MonitorInfo);

				if (!::GetMonitorInfo(Monitor, &MonitorInfo))
				{
					return true;
				}

				ImGuiPlatformMonitor PlatformMonitor;
				PlatformMonitor.MainPos = { static_cast<float>(MonitorInfo.rcMonitor.left), static_cast<float>(MonitorInfo.rcMonitor.top) };
				PlatformMonitor.MainSize = { static_cast<float>(MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left), static_cast<float>(MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top) };
				PlatformMonitor.WorkPos = { static_cast<float>(MonitorInfo.rcWork.left), static_cast<float>(MonitorInfo.rcWork.top) };
				PlatformMonitor.WorkSize = { static_cast<float>(MonitorInfo.rcWork.right - MonitorInfo.rcWork.left), static_cast<float>(MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top) };
				PlatformMonitor.DpiScale = GetDPIScale(Monitor);

				auto& PlatformIO = ImGui::GetPlatformIO();

				if (MonitorInfo.dwFlags & MONITORINFOF_PRIMARY)
				{
					PlatformIO.Monitors.push_front(PlatformMonitor);
				}

				else
				{
					PlatformIO.Monitors.push_back(PlatformMonitor);
				}

				return true;
			}), 0);
	}

	// #TODO: Implement viewports features.

	void UpdateKeyboard()
	{
		auto& IO = ImGui::GetIO();

		// Update key modifiers that aren't handled by the input processing.
		IO.KeyCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
		IO.KeyShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
		IO.KeyAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
		IO.KeySuper = false;
	}

	void UpdateMouse(void* Window)
	{
		auto& IO = ImGui::GetIO();

		if (IO.WantSetMousePos)
		{
			POINT TargetPoint = { static_cast<int>(IO.MousePos.x), static_cast<int>(IO.MousePos.y) };

			::SetCursorPos(TargetPoint.x, TargetPoint.y);
		}

		IO.MousePos.x = std::numeric_limits<float>::min();
		IO.MousePos.y = std::numeric_limits<float>::min();
		IO.MouseHoveredViewport = 0;

		POINT MousePosition;
		if (!::GetCursorPos(&MousePosition))
		{
			VGLogWarning(Core) << "Failed to get mouse cursor position: " << GetPlatformError();

			return;
		}

		if (auto* ForegroundWindow = ::GetForegroundWindow(); ForegroundWindow)
		{
			if (::IsChild(ForegroundWindow, static_cast<HWND>(Window)))
			{
				ForegroundWindow = static_cast<HWND>(Window);
			}

			if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				if (ImGui::FindViewportByPlatformHandle(ForegroundWindow))
				{
					IO.MousePos = { static_cast<float>(MousePosition.x), static_cast<float>(MousePosition.y) };
				}
			}

			else
			{
				if (ForegroundWindow == Window)
				{
					if (!::ScreenToClient(static_cast<HWND>(Window), &MousePosition))
						VGLogWarning(Core) << "Failed to convert mouse position from screen space to window space: " << GetPlatformError();
					else
						IO.MousePos = { static_cast<float>(MousePosition.x), static_cast<float>(MousePosition.y) };
				}
			}
		}

		if (auto HoveredWindow = ::WindowFromPoint(MousePosition); HoveredWindow)
		{
			if (auto* ImViewport = ImGui::FindViewportByPlatformHandle(HoveredWindow); ImViewport)
			{
				if (!(ImViewport->Flags & ImGuiViewportFlags_NoInputs))
				{
					IO.MouseHoveredViewport = ImViewport->ID;
				}
			}
		}

		auto Cursor = ImGui::GetMouseCursor();

		if (Cursor == ImGuiMouseCursor_None || IO.MouseDrawCursor)
		{
			::SetCursor(nullptr);  // Hide the cursor.
		}

		else
		{
			// Default to arrow.
			LPTSTR PlatformCursor = IDC_ARROW;

			switch (Cursor)
			{
			case ImGuiMouseCursor_Arrow: PlatformCursor = IDC_ARROW; break;
			case ImGuiMouseCursor_TextInput: PlatformCursor = IDC_IBEAM; break;
			case ImGuiMouseCursor_ResizeAll: PlatformCursor = IDC_SIZEALL; break;
			case ImGuiMouseCursor_ResizeEW: PlatformCursor = IDC_SIZEWE; break;
			case ImGuiMouseCursor_ResizeNS: PlatformCursor = IDC_SIZENS; break;
			case ImGuiMouseCursor_ResizeNESW: PlatformCursor = IDC_SIZENESW; break;
			case ImGuiMouseCursor_ResizeNWSE: PlatformCursor = IDC_SIZENWSE; break;
			case ImGuiMouseCursor_Hand: PlatformCursor = IDC_HAND; break;
			case ImGuiMouseCursor_NotAllowed: PlatformCursor = IDC_NO; break;
			}

			if (!::SetCursor(::LoadCursor(nullptr, PlatformCursor)))
			{
				VGLogWarning(Core) << "Failed to set cursor: " << GetPlatformError();
			}
		}
	}

	void UpdateGamepad()
	{
		// #TODO: Implement support for gamepads.
	}

	void Initialize(void* Window)
	{
		// Ensure we have an ImGui context.
		if (!ImGui::GetCurrentContext())
		{
			VGLogFatal(Core) << "Missing ImGui context!";
		}

		auto& IO = ImGui::GetIO();

		IO.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		IO.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
		IO.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
		IO.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
		IO.BackendPlatformName = "ImGui Win64";

		ImGui::GetMainViewport()->PlatformHandleRaw = Window;

		IO.KeyMap[ImGuiKey_Tab] = VK_TAB;
		IO.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
		IO.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
		IO.KeyMap[ImGuiKey_UpArrow] = VK_UP;
		IO.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
		IO.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
		IO.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
		IO.KeyMap[ImGuiKey_Home] = VK_HOME;
		IO.KeyMap[ImGuiKey_End] = VK_END;
		IO.KeyMap[ImGuiKey_Insert] = VK_INSERT;
		IO.KeyMap[ImGuiKey_Delete] = VK_DELETE;
		IO.KeyMap[ImGuiKey_Backspace] = VK_BACK;
		IO.KeyMap[ImGuiKey_Space] = VK_SPACE;
		IO.KeyMap[ImGuiKey_Enter] = VK_RETURN;
		IO.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
		IO.KeyMap[ImGuiKey_KeyPadEnter] = VK_RETURN;
		IO.KeyMap[ImGuiKey_A] = 'A';
		IO.KeyMap[ImGuiKey_C] = 'C';
		IO.KeyMap[ImGuiKey_V] = 'V';
		IO.KeyMap[ImGuiKey_X] = 'X';
		IO.KeyMap[ImGuiKey_Y] = 'Y';
		IO.KeyMap[ImGuiKey_Z] = 'Z';
	}

	void EnableDPIAwareness()
	{
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	}

	bool ProcessWindowMessage(void* Window, uint32_t Message, int64_t wParam, uint64_t lParam)
	{
		// Ensure we have an ImGui context.
		if (!ImGui::GetCurrentContext())
			return false;

		auto& IO = ImGui::GetIO();

		switch (Message)
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
			size_t MouseButton = 0;  // Default to left click.

			if (Message == WM_RBUTTONDOWN || Message == WM_RBUTTONDBLCLK) MouseButton = 1;
			if (Message == WM_MBUTTONDOWN || Message == WM_MBUTTONDBLCLK) MouseButton = 2;
			if (Message == WM_XBUTTONDOWN || Message == WM_XBUTTONDBLCLK) MouseButton = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4;

			if (!ImGui::IsAnyMouseDown() && !::GetCapture())
			{
				::SetCapture(static_cast<HWND>(Window));
			}

			IO.MouseDown[MouseButton] = true;

			return true;
		}

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
		{
			size_t MouseButton = 0;  // Default to left click.

			if (Message == WM_RBUTTONDOWN || Message == WM_RBUTTONDBLCLK) MouseButton = 1;
			if (Message == WM_MBUTTONDOWN || Message == WM_MBUTTONDBLCLK) MouseButton = 2;
			if (Message == WM_XBUTTONDOWN || Message == WM_XBUTTONDBLCLK) MouseButton = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4;

			IO.MouseDown[MouseButton] = false;

			if (!ImGui::IsAnyMouseDown() && ::GetCapture() == Window)
			{
				::ReleaseCapture();
			}

			return true;
		}

		// Mouse scroll events.

		case WM_MOUSEWHEEL:
		{
			IO.MouseWheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);

			return true;
		}

		case WM_MOUSEHWHEEL:
		{
			IO.MouseWheelH += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);

			return true;
		}

		// Keyboard events.

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			if (wParam < 256)
			{
				IO.KeysDown[wParam] = true;

				return true;
			}

			return false;  // We don't handle these keys.
		}

		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			if (wParam < 256)
			{
				IO.KeysDown[wParam] = false;

				return true;
			}

			return false;  // We don't handle these keys.
		}

		case WM_CHAR:
		{
			if (wParam > 0 && wParam < 0x10000)
			{
				IO.AddInputCharacterUTF16(static_cast<unsigned short>(wParam));

				return true;
			}

			return false;
		}

		// Display events.

		case WM_DISPLAYCHANGE:
		{
			PendingMonitorUpdate = true;

			return true;
		}
		}

		return false;
	}

	void UpdateInputDevices(void* Window)
	{
		VGScopedCPUStat("Update Input Devices");

		// If we have a monitor update, run it before any mouse-related tasks.
		if (PendingMonitorUpdate)
		{
			UpdateMonitors();
		}

		UpdateKeyboard();
		UpdateMouse(Window);
		UpdateGamepad();
	}
}