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
#include <Rendering/RenderComponents.h>
#include <Core/CoreComponents.h>

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

void EngineBoot()
{
	VGScopedCPUStat("Engine Boot");

	LogManager::Get().AddOutput<DefaultLogOutput>();

	EnableDPIAwareness();

	MainWindow = std::make_unique<WindowFrame>(VGText("Vanguard Engine"), 800, 600, &OnFocusChanged);
	MainWindow->RestrainCursor(false);

#if BUILD_DEBUG
	constexpr auto EnableDebugging = true;
#else
	constexpr auto EnableDebugging = false;
#endif

	auto Device = std::make_unique<RenderDevice>(static_cast<HWND>(MainWindow->GetHandle()), false, EnableDebugging);
	Renderer::Get().Initialize(std::move(Device));
}

void EngineLoop()
{
	entt::registry TempReg;
	
	for (auto Index = 0; Index < 10; ++Index)
	{
		const auto Entity = TempReg.create();
		TempReg.assign<TransformComponent>(Entity);
		TempReg.assign<MeshComponent>(Entity);
	}

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

		// #TEMP: Testing resource management.
		auto Comp{ CreateMeshComponent(*Renderer::Get().Device, std::vector<Vertex>{ Vertex{}, Vertex{}, Vertex{} }, std::vector<uint32_t>{ 0, 1, 2 }) };

		Renderer::Get().Render(TempReg);

		// #TEMP
		std::this_thread::sleep_for(4ms);

		Renderer::Get().Device->FrameStep();
	}
}

void EngineShutdown()
{
	VGScopedCPUStat("Engine Shutdown");

	MainWindow.reset();
}

int32_t EngineMain()
{
	EngineBoot();

	EngineLoop();

	EngineShutdown();

	return 0;
}