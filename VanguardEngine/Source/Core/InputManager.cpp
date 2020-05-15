// Copyright (c) 2019-2020 Andrew Depke

#include <Core/InputManager.h>
#include <Window/WindowFrame.h>

#include <imgui.h>

#include <limits>

#include <Core/Windows/WindowsMinimal.h>
#include <XInput.h>  // Required for gamepad support.

// #TODO: Implement viewports features.

void InputManager::UpdateKeyboard()
{
	auto& IO = ImGui::GetIO();

	// Update key modifiers that aren't handled by the input processing.
	IO.KeyCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
	IO.KeyShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
	IO.KeyAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
	IO.KeySuper = false;
}

void InputManager::UpdateMouse()
{
	auto& IO = ImGui::GetIO();

	if (IO.WantSetMousePos)
	{
		POINT TargetPoint = { static_cast<int>(IO.MousePos.x), static_cast<int>(IO.MousePos.y) };

		::ClientToScreen(static_cast<HWND>(WindowFrame::Get().GetHandle()), &TargetPoint);  // Convert the point to screen space.
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
		if (::IsChild(ForegroundWindow, static_cast<HWND>(WindowFrame::Get().GetHandle())))
		{
			ForegroundWindow = static_cast<HWND>(WindowFrame::Get().GetHandle());
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
			if (ForegroundWindow == WindowFrame::Get().GetHandle())
			{
				if (!::ScreenToClient(static_cast<HWND>(WindowFrame::Get().GetHandle()), &MousePosition))
					VGLogWarning(Core) << "Failed to convert mouse position from screen space to window space: " << GetPlatformError();
				else
					IO.MousePos = { static_cast<float>(MousePosition.x), static_cast<float>(MousePosition.y) };
			}
		}
	}

	// #TODO: Handle mouse icons.
}

void InputManager::UpdateGamepad()
{
	// #TODO: Implement support for gamepads.
}

InputManager::InputManager()
{
	// Ensure we have an ImGui context.
	if (!ImGui::GetCurrentContext())
		return;

	auto& IO = ImGui::GetIO();

	IO.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	IO.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
	IO.BackendPlatformName = "ImGui Win32";

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

bool InputManager::ProcessWindowMessage(uint32_t Message, int64_t wParam, uint64_t lParam)
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
			::SetCapture(static_cast<HWND>(WindowFrame::Get().GetHandle()));
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

		if (!ImGui::IsAnyMouseDown() && ::GetCapture() == WindowFrame::Get().GetHandle())
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
	}

	return false;
}

void InputManager::UpdateInputDevices()
{
	VGScopedCPUStat("Update Input Devices");

	UpdateKeyboard();
	UpdateMouse();
	UpdateGamepad();
}