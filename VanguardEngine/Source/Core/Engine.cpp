// Copyright (c) 2019 Andrew Depke

#include <Core/Engine.h>
#include <Core/LogOutputs.h>
#include <Core/Config.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Window/WindowFrame.h>

#include <string>
// #TEMP
#include <chrono>
#include <thread>

std::unique_ptr<WindowFrame> MainWindow;

void EnableDPIAwareness()
{
	VGScopedCPUStat("Enable DPI Awareness");

	// #TODO
}

void OnFocusChanged(bool Focus)
{
	VGScopedCPUStat("Focus Changed");

	VGLog(Window) << (Focus ? "Acquired focus." : "Released focus.");

	// #TODO: Limit render FPS, disable audio.
}

void EngineLoop()
{
	while (true)
	{
		{
			VGScopedCPUStat("Window Message Processing");

			MSG Message{};
			if (PeekMessage(&Message, static_cast<HWND>(MainWindow->GetHandle()), 0, 0, PM_REMOVE))
			{
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}

			if (Message.message == WM_QUIT)
			{
				return;
			}
		}

		// #TEMP
		std::this_thread::sleep_for(16ms);

		VGStatFrame;
	}
}

int32_t EngineMain()
{
	{
		VGScopedCPUStat("Engine Boot");

		LogManager::Get().AddOutput<DefaultLogOutput>();

		EnableDPIAwareness();

		MainWindow = std::make_unique<WindowFrame>(VGText("Vanguard Engine"), 800, 600, &OnFocusChanged);
		MainWindow->RestrainCursor(true);

#if BUILD_DEBUG
		constexpr auto EnableDebugging = true;
#else
		constexpr auto EnableDebugging = false;
#endif

		auto Device = std::make_unique<RenderDevice>(static_cast<HWND>(MainWindow->GetHandle()), false, EnableDebugging);
		Renderer::Get().Initialize(std::move(Device));
	}

	EngineLoop();

	EngineShutdown();

	return 0;
}

void EngineShutdown()
{
	VGScopedCPUStat("Engine Shutdown");

	MainWindow.reset();
}