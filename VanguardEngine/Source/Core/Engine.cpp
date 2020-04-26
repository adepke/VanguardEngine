// Copyright (c) 2019-2020 Andrew Depke

#include <Core/Engine.h>
#include <Core/LogOutputs.h>
#include <Core/Config.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Window/WindowFrame.h>

#include <string>
// #TEMP
#include <thread>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
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

void OnSizeChanged(size_t Width, size_t Height, bool Fullscreen)
{
	VGScopedCPUStat("Size Changed");

	VGLog(Window) << "Window size changed (" << Width << ", " << Height << ").";

	Renderer::Get().Device->SetResolution(Width, Height, Fullscreen);
}

void EngineBoot()
{
	VGScopedCPUStat("Engine Boot");

	LogManager::Get().AddOutput<DefaultLogOutput>();

	EnableDPIAwareness();

	constexpr auto DefaultWindowSizeX = 800;
	constexpr auto DefaultWindowSizeY = 600;

	MainWindow = std::make_unique<WindowFrame>(VGText("Vanguard Engine"), DefaultWindowSizeX, DefaultWindowSizeY);
	MainWindow->OnFocusChanged = &OnFocusChanged;
	MainWindow->OnSizeChanged = &OnSizeChanged;
	MainWindow->RestrainCursor(false);

#if BUILD_DEBUG
	constexpr auto EnableDebugging = true;
#else
	constexpr auto EnableDebugging = false;
#endif

	auto Device = std::make_unique<RenderDevice>(static_cast<HWND>(MainWindow->GetHandle()), false, EnableDebugging);
	Device->SetResolution(DefaultWindowSizeX, DefaultWindowSizeY, false);
	Renderer::Get().Initialize(std::move(Device));
}

void EngineLoop()
{
	entt::registry TempReg;

	TransformComponent SpectatorTransform{};
	SpectatorTransform.Translation.x = 0.f;
	SpectatorTransform.Translation.y = 0.f;
	SpectatorTransform.Translation.z = -3.f;

	CameraComponent SpectatorCamera{};
	SpectatorCamera.FocusPosition = { 0.f, 0.f, 0.f };
	SpectatorCamera.UpDirection = { 0.f, 1.f, 0.f };

	const auto Spectator = TempReg.create();
	TempReg.assign<TransformComponent>(Spectator, std::move(SpectatorTransform));
	TempReg.assign<CameraComponent>(Spectator, std::move(SpectatorCamera));

	for (int Index = 0; Index < 1; ++Index)
	{
		Vertex v1{ { 0.f, 0.5f, 0.f }, { 1.f, 0.f, 0.f, 1.f } };
		Vertex v2{ { 0.5f, -0.5f, 0.f }, { 0.f, 1.f, 0.f, 1.f } };
		Vertex v3{ { -0.5f, -0.5f, 0.f }, { 0.f, 0.f, 1.f, 1.f } };
		auto Mesh{ CreateMeshComponent(*Renderer::Get().Device, std::vector<Vertex>{ v1, v2, v3 }, std::vector<uint32_t>{ 0, 1, 2 }) };

		const auto Entity = TempReg.create();
		TempReg.assign<TransformComponent>(Entity);
		TempReg.assign<MeshComponent>(Entity, std::move(Mesh));
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

		CameraSystem::Update(TempReg);

		Renderer::Get().Render(TempReg);

		Renderer::Get().Device->AdvanceCPU();
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