// Copyright (c) 2019-2022 Andrew Depke

#include <Core/Input.h>
#include <Window/WindowFrame.h>

#include <imgui.h>

#include <limits>

#include <Core/Windows/WindowsMinimal.h>
#include <XInput.h>  // Required for gamepad support.
#include <ShellScalingApi.h>

namespace Input
{
	// ImGui helper functions from the official win32 backend.

	ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM wParam, LPARAM lParam)
	{
		// There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED.
		if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
			return ImGuiKey_KeypadEnter;

		switch (wParam)
		{
		case VK_TAB: return ImGuiKey_Tab;
		case VK_LEFT: return ImGuiKey_LeftArrow;
		case VK_RIGHT: return ImGuiKey_RightArrow;
		case VK_UP: return ImGuiKey_UpArrow;
		case VK_DOWN: return ImGuiKey_DownArrow;
		case VK_PRIOR: return ImGuiKey_PageUp;
		case VK_NEXT: return ImGuiKey_PageDown;
		case VK_HOME: return ImGuiKey_Home;
		case VK_END: return ImGuiKey_End;
		case VK_INSERT: return ImGuiKey_Insert;
		case VK_DELETE: return ImGuiKey_Delete;
		case VK_BACK: return ImGuiKey_Backspace;
		case VK_SPACE: return ImGuiKey_Space;
		case VK_RETURN: return ImGuiKey_Enter;
		case VK_ESCAPE: return ImGuiKey_Escape;
		case VK_OEM_7: return ImGuiKey_Apostrophe;
		case VK_OEM_COMMA: return ImGuiKey_Comma;
		case VK_OEM_MINUS: return ImGuiKey_Minus;
		case VK_OEM_PERIOD: return ImGuiKey_Period;
		case VK_OEM_2: return ImGuiKey_Slash;
		case VK_OEM_1: return ImGuiKey_Semicolon;
		case VK_OEM_PLUS: return ImGuiKey_Equal;
		case VK_OEM_4: return ImGuiKey_LeftBracket;
		case VK_OEM_5: return ImGuiKey_Backslash;
		case VK_OEM_6: return ImGuiKey_RightBracket;
		case VK_OEM_3: return ImGuiKey_GraveAccent;
		case VK_CAPITAL: return ImGuiKey_CapsLock;
		case VK_SCROLL: return ImGuiKey_ScrollLock;
		case VK_NUMLOCK: return ImGuiKey_NumLock;
		case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
		case VK_PAUSE: return ImGuiKey_Pause;
		case VK_NUMPAD0: return ImGuiKey_Keypad0;
		case VK_NUMPAD1: return ImGuiKey_Keypad1;
		case VK_NUMPAD2: return ImGuiKey_Keypad2;
		case VK_NUMPAD3: return ImGuiKey_Keypad3;
		case VK_NUMPAD4: return ImGuiKey_Keypad4;
		case VK_NUMPAD5: return ImGuiKey_Keypad5;
		case VK_NUMPAD6: return ImGuiKey_Keypad6;
		case VK_NUMPAD7: return ImGuiKey_Keypad7;
		case VK_NUMPAD8: return ImGuiKey_Keypad8;
		case VK_NUMPAD9: return ImGuiKey_Keypad9;
		case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
		case VK_DIVIDE: return ImGuiKey_KeypadDivide;
		case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
		case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
		case VK_ADD: return ImGuiKey_KeypadAdd;
		case VK_LSHIFT: return ImGuiKey_LeftShift;
		case VK_LCONTROL: return ImGuiKey_LeftCtrl;
		case VK_LMENU: return ImGuiKey_LeftAlt;
		case VK_LWIN: return ImGuiKey_LeftSuper;
		case VK_RSHIFT: return ImGuiKey_RightShift;
		case VK_RCONTROL: return ImGuiKey_RightCtrl;
		case VK_RMENU: return ImGuiKey_RightAlt;
		case VK_RWIN: return ImGuiKey_RightSuper;
		case VK_APPS: return ImGuiKey_Menu;
		case '0': return ImGuiKey_0;
		case '1': return ImGuiKey_1;
		case '2': return ImGuiKey_2;
		case '3': return ImGuiKey_3;
		case '4': return ImGuiKey_4;
		case '5': return ImGuiKey_5;
		case '6': return ImGuiKey_6;
		case '7': return ImGuiKey_7;
		case '8': return ImGuiKey_8;
		case '9': return ImGuiKey_9;
		case 'A': return ImGuiKey_A;
		case 'B': return ImGuiKey_B;
		case 'C': return ImGuiKey_C;
		case 'D': return ImGuiKey_D;
		case 'E': return ImGuiKey_E;
		case 'F': return ImGuiKey_F;
		case 'G': return ImGuiKey_G;
		case 'H': return ImGuiKey_H;
		case 'I': return ImGuiKey_I;
		case 'J': return ImGuiKey_J;
		case 'K': return ImGuiKey_K;
		case 'L': return ImGuiKey_L;
		case 'M': return ImGuiKey_M;
		case 'N': return ImGuiKey_N;
		case 'O': return ImGuiKey_O;
		case 'P': return ImGuiKey_P;
		case 'Q': return ImGuiKey_Q;
		case 'R': return ImGuiKey_R;
		case 'S': return ImGuiKey_S;
		case 'T': return ImGuiKey_T;
		case 'U': return ImGuiKey_U;
		case 'V': return ImGuiKey_V;
		case 'W': return ImGuiKey_W;
		case 'X': return ImGuiKey_X;
		case 'Y': return ImGuiKey_Y;
		case 'Z': return ImGuiKey_Z;
		case VK_F1: return ImGuiKey_F1;
		case VK_F2: return ImGuiKey_F2;
		case VK_F3: return ImGuiKey_F3;
		case VK_F4: return ImGuiKey_F4;
		case VK_F5: return ImGuiKey_F5;
		case VK_F6: return ImGuiKey_F6;
		case VK_F7: return ImGuiKey_F7;
		case VK_F8: return ImGuiKey_F8;
		case VK_F9: return ImGuiKey_F9;
		case VK_F10: return ImGuiKey_F10;
		case VK_F11: return ImGuiKey_F11;
		case VK_F12: return ImGuiKey_F12;
		case VK_F13: return ImGuiKey_F13;
		case VK_F14: return ImGuiKey_F14;
		case VK_F15: return ImGuiKey_F15;
		case VK_F16: return ImGuiKey_F16;
		case VK_F17: return ImGuiKey_F17;
		case VK_F18: return ImGuiKey_F18;
		case VK_F19: return ImGuiKey_F19;
		case VK_F20: return ImGuiKey_F20;
		case VK_F21: return ImGuiKey_F21;
		case VK_F22: return ImGuiKey_F22;
		case VK_F23: return ImGuiKey_F23;
		case VK_F24: return ImGuiKey_F24;
		case VK_BROWSER_BACK: return ImGuiKey_AppBack;
		case VK_BROWSER_FORWARD: return ImGuiKey_AppForward;
		default: return ImGuiKey_None;
		}
	}

	static void ImGui_ImplWin32_AddKeyEvent(ImGuiIO& io, ImGuiKey key, bool down, int native_keycode, int native_scancode = -1)
	{
		io.AddKeyEvent(key, down);
		io.SetKeyEventNativeData(key, native_keycode, native_scancode); // To support legacy indexing (<1.87 user code)
		IM_UNUSED(native_scancode);
	}

	static ImGuiViewport* ImGui_ImplWin32_FindViewportByPlatformHandle(ImGuiPlatformIO& platform_io, HWND hwnd)
	{
		// We cannot use ImGui::FindViewportByPlatformHandle() because it doesn't take a context.
		// When called from ImGui_ImplWin32_WndProcHandler_PlatformWindow() we don't assume that context is bound.
		//return ImGui::FindViewportByPlatformHandle((void*)hwnd);
		for (ImGuiViewport* viewport : platform_io.Viewports)
			if (viewport->PlatformHandle == hwnd)
				return viewport;
		return nullptr;
	}

	// End of ImGui backend functions

	static bool pendingMonitorUpdate = true;
	static int mouseTrackedArea = 0;  // Track all mouse movements

	float GetDPIScale(void* monitor)
	{
		uint32_t dpiX = 96;
		uint32_t dpiY = 96;

		if (FAILED(::GetDpiForMonitor(static_cast<HMONITOR>(monitor), MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
		{
			VGLogError(logCore, "Failed to get monitor DPI.");

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
		auto& platform_io = ImGui::GetPlatformIO();
		const HWND hWnd = static_cast<HWND>(window);

		// Taken from ImGui official win32 backend.

		POINT mouse_screen_pos;
		bool has_mouse_screen_pos = ::GetCursorPos(&mouse_screen_pos) != 0;

		HWND focused_window = ::GetForegroundWindow();
		const bool is_app_focused = (focused_window && (focused_window == hWnd || ::IsChild(focused_window, hWnd) || ImGui_ImplWin32_FindViewportByPlatformHandle(platform_io, focused_window)));

		if (is_app_focused)
		{
			// (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when io.ConfigNavMoveSetMousePos is enabled by user)
			// When multi-viewports are enabled, all Dear ImGui positions are same as OS positions.
			if (io.WantSetMousePos)
			{
				POINT targetPoint = { static_cast<int>(io.MousePos.x), static_cast<int>(io.MousePos.y) };

				if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) == 0)
					::ClientToScreen(focused_window, &targetPoint);
				::SetCursorPos(targetPoint.x, targetPoint.y);
			}

			// (Optional) Fallback to provide mouse position when focused (WM_MOUSEMOVE already provides this when hovered or captured)
			// This also fills a short gap when clicking non-client area: WM_NCMOUSELEAVE -> modal OS move -> gap -> WM_NCMOUSEMOVE
			if (!io.WantSetMousePos && mouseTrackedArea == 0 && has_mouse_screen_pos)
			{
				// Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app window)
				// (This is the position you can get with ::GetCursorPos() + ::ScreenToClient() or WM_MOUSEMOVE.)
				// Multi-viewport mode: mouse position in OS absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary monitor)
				// (This is the position you can get with ::GetCursorPos() or WM_MOUSEMOVE + ::ClientToScreen(). In theory adding viewport->Pos to a client position would also be the same.)
				POINT mouse_pos = mouse_screen_pos;
				if (!(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
					::ScreenToClient(hWnd, &mouse_pos);
				io.AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
			}
		}

		// (Optional) When using multiple viewports: call io.AddMouseViewportEvent() with the viewport the OS mouse cursor is hovering.
		// If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the backend, Dear imGui will ignore this field and infer the information using its flawed heuristic.
		// - [X] Win32 backend correctly ignore viewports with the _NoInputs flag (here using ::WindowFromPoint with WM_NCHITTEST + HTTRANSPARENT in WndProc does that)
		//       Some backend are not able to handle that correctly. If a backend report an hovered viewport that has the _NoInputs flag (e.g. when dragging a window
		//       for docking, the viewport has the _NoInputs flag in order to allow us to find the viewport under), then Dear ImGui is forced to ignore the value reported
		//       by the backend, and use its flawed heuristic to guess the viewport behind.
		// - [X] Win32 backend correctly reports this regardless of another viewport behind focused and dragged from (we need this to find a useful drag and drop target).
		ImGuiID mouse_viewport_id = 0;
		if (has_mouse_screen_pos)
			if (HWND hovered_hwnd = ::WindowFromPoint(mouse_screen_pos))
				if (ImGuiViewport* viewport = ImGui_ImplWin32_FindViewportByPlatformHandle(platform_io, hovered_hwnd))
					mouse_viewport_id = viewport->ID;
		io.AddMouseViewportEvent(mouse_viewport_id);

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
				VGLogWarning(logCore, "Failed to set cursor: {}", GetPlatformError());
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
			VGLogCritical(logCore, "Missing ImGui context!");
		}

		auto& io = ImGui::GetIO();

		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
		io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
		io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
		io.BackendPlatformName = "Vanguard Win64";

		ImGui::GetMainViewport()->PlatformHandleRaw = window;
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
		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			if (wParam < 256)
			{
				constexpr auto IsVkDown = [](int vk)
				{
					return (::GetKeyState(vk) & 0x8000) != 0;
				};

				const bool isKeyDown = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);

				// Submit modifiers
				io.AddKeyEvent(ImGuiMod_Ctrl, IsVkDown(VK_CONTROL));
				io.AddKeyEvent(ImGuiMod_Shift, IsVkDown(VK_SHIFT));
				io.AddKeyEvent(ImGuiMod_Alt, IsVkDown(VK_MENU));
				io.AddKeyEvent(ImGuiMod_Super, IsVkDown(VK_LWIN) || IsVkDown(VK_RWIN));

				// Obtain virtual key code and convert to ImGuiKey
				const ImGuiKey key = ImGui_ImplWin32_KeyEventToImGuiKey(wParam, lParam);
				const int vk = (int)wParam;
				const int scancode = (int)LOBYTE(HIWORD(lParam));

				// Special behavior for VK_SNAPSHOT / ImGuiKey_PrintScreen as Windows doesn't emit the key down event.
				if (key == ImGuiKey_PrintScreen && !isKeyDown)
					ImGui_ImplWin32_AddKeyEvent(io, key, true, vk, scancode);

				// Submit key event
				if (key != ImGuiKey_None)
					ImGui_ImplWin32_AddKeyEvent(io, key, isKeyDown, vk, scancode);

				// Submit individual left/right modifier events
				if (vk == VK_SHIFT)
				{
					// Important: Shift keys tend to get stuck when pressed together, missing key-up events are corrected in ImGui_ImplWin32_ProcessKeyEventsWorkarounds()
					if (IsVkDown(VK_LSHIFT) == isKeyDown) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_LeftShift, isKeyDown, VK_LSHIFT, scancode); }
					if (IsVkDown(VK_RSHIFT) == isKeyDown) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_RightShift, isKeyDown, VK_RSHIFT, scancode); }
				}
				else if (vk == VK_CONTROL)
				{
					if (IsVkDown(VK_LCONTROL) == isKeyDown) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_LeftCtrl, isKeyDown, VK_LCONTROL, scancode); }
					if (IsVkDown(VK_RCONTROL) == isKeyDown) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_RightCtrl, isKeyDown, VK_RCONTROL, scancode); }
				}
				else if (vk == VK_MENU)
				{
					if (IsVkDown(VK_LMENU) == isKeyDown) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_LeftAlt, isKeyDown, VK_LMENU, scancode); }
					if (IsVkDown(VK_RMENU) == isKeyDown) { ImGui_ImplWin32_AddKeyEvent(io, ImGuiKey_RightAlt, isKeyDown, VK_RMENU, scancode); }
				}

				return true;
			}

			return false;  // We don't handle these keys.
		}

		case WM_CHAR:
		{
			// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
			if (wParam > 0 && wParam < 0x10000)
				io.AddInputCharacterUTF16((unsigned short)wParam);

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

	void SubmitFrameTime(uint32_t timeUs)
	{
		auto& io = ImGui::GetIO();
		io.DeltaTime = (float)timeUs / 1000000.f;
	}
}