// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>

class WindowFrame;

class InputManager
{
private:
	std::shared_ptr<WindowFrame> Window;

	void UpdateKeyboard();
	void UpdateMouse();
	void UpdateGamepad();

public:
	static inline InputManager& Get() noexcept
	{
		static InputManager Singleton;
		return Singleton;
	}

	InputManager();
	InputManager(const InputManager&) = delete;
	InputManager(InputManager&&) noexcept = delete;

	InputManager& operator=(const InputManager&) = delete;
	InputManager& operator=(InputManager&&) noexcept = delete;

	bool ProcessWindowMessage(uint32_t Message, int64_t wParam, uint64_t lParam);
	void UpdateInputDevices();
};