// Copyright (c) 2019-2020 Andrew Depke

#include <Core/Engine.h>
#include <Core/LogOutputs.h>
#include <Core/Config.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Window/WindowFrame.h>
#include <Core/Input.h>
#include <Core/CoreComponents.h>
#include <Core/CoreSystems.h>

#include <string>
#include <memory>

// #TEMP
#include <thread>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Asset/AssetLoader.h>
//

void OnFocusChanged(bool Focus)
{
	VGScopedCPUStat("Focus Changed");

	VGLog(Window) << (Focus ? "Acquired focus." : "Released focus.");

	// #TODO: Limit render FPS, disable audio.
}

void OnSizeChanged(uint32_t Width, uint32_t Height, bool Fullscreen)
{
	VGScopedCPUStat("Size Changed");

	VGLog(Window) << "Window size changed (" << Width << ", " << Height << ").";

	Renderer::Get().Device->SetResolution(Width, Height, Fullscreen);
}

void EngineBoot()
{
	VGScopedCPUStat("Engine Boot");

	// Send the output to the profiler and to the VS debugger.
	Logger::Get().AddOutput<LogWindowsOutput>();
	Logger::Get().AddOutput<LogProfilerOutput>();

	Config::Initialize();

	Input::EnableDPIAwareness();

	constexpr uint32_t DefaultWindowSizeX = 1200;
	constexpr uint32_t DefaultWindowSizeY = 900;

	auto Window = std::make_unique<WindowFrame>(VGText("Vanguard Engine"), DefaultWindowSizeX, DefaultWindowSizeY);
	Window->OnFocusChanged = &OnFocusChanged;
	Window->OnSizeChanged = &OnSizeChanged;

#if BUILD_DEBUG
	constexpr auto EnableDebugging = true;
#else
	constexpr auto EnableDebugging = false;
#endif

	auto Device = std::make_unique<RenderDevice>(static_cast<HWND>(Window->GetHandle()), false, EnableDebugging);
	Device->SetResolution(DefaultWindowSizeX, DefaultWindowSizeY, false);

	Renderer::Get().Initialize(std::move(Window), std::move(Device));

	// The input requires the user interface to be created first.
	Input::Initialize(Renderer::Get().Window->GetHandle());
}

void EngineLoop()
{
	entt::registry TempReg;

	TransformComponent SpectatorTransform{};
	SpectatorTransform.Translation.x = -10.f;
	SpectatorTransform.Translation.y = 0.f;
	SpectatorTransform.Translation.z = 1.f;

	const auto Spectator = TempReg.create();
	TempReg.emplace<NameComponent>(Spectator, "Spectator");
	TempReg.emplace<TransformComponent>(Spectator, std::move(SpectatorTransform));
	TempReg.emplace<CameraComponent>(Spectator);
	TempReg.emplace<ControlComponent>(Spectator);  // #TEMP

	const auto Sponza = TempReg.create();
	TempReg.emplace<NameComponent>(Sponza, "Sponza");
	TempReg.emplace<TransformComponent>(Sponza);
	TempReg.emplace<MeshComponent>(Sponza, AssetLoader::LoadMesh(*Renderer::Get().Device, Config::ShadersPath / "../Models/Sponza.obj"));  // #TEMP

	while (true)
	{
		{
			VGScopedCPUStat("Window Message Processing");

			MSG Message{};
			if (::PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
			{
				::TranslateMessage(&Message);
				::DispatchMessage(&Message);
			}

			if (Message.message == WM_QUIT)
			{
				return;
			}
		}

		ControlSystem::Update(TempReg);

		CameraSystem::Update(TempReg);

		Renderer::Get().Render(TempReg);

		Renderer::Get().Device->AdvanceCPU();
	}
}

void EngineShutdown()
{
	VGScopedCPUStat("Engine Shutdown");

	VGLog(Core) << "Engine shutting down.";
}

int32_t EngineMain()
{
	EngineBoot();

	EngineLoop();

	EngineShutdown();

	return 0;
}