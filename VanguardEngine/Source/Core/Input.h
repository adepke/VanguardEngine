// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <cstdint>

namespace Input
{
	void Initialize(void* Window);

	void EnableDPIAwareness();

	bool ProcessWindowMessage(void* Window, uint32_t Message, int64_t wParam, uint64_t lParam);
	void UpdateInputDevices(void* Window);
};