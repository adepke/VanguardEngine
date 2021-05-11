// Copyright (c) 2019-2021 Andrew Depke

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
#include <chrono>

// #TEMP
#include <thread>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Asset/AssetLoader.h>
//

void OnFocusChanged(bool focus)
{
	VGScopedCPUStat("Focus Changed");

	VGLog(Window) << (focus ? "Acquired focus." : "Released focus.");

	// #TODO: Limit render FPS, disable audio.
}

void OnSizeChanged(uint32_t width, uint32_t height, bool fullscreen)
{
	VGScopedCPUStat("Size Changed");

	VGLog(Window) << "Window size changed (" << width << ", " << height << ").";

	Renderer::Get().device->SetResolution(width, height, fullscreen);
	Renderer::Get().DiscardRenderData();
}

void EngineBoot()
{
	VGScopedCPUStat("Engine Boot");

	// Send the output to the profiler and to the VS debugger.
	Logger::Get().AddOutput<LogWindowsOutput>();
	Logger::Get().AddOutput<LogProfilerOutput>();

	Config::Initialize();

	Input::EnableDPIAwareness();

	constexpr uint32_t defaultWindowSizeX = 1200;
	constexpr uint32_t defaultWindowSizeY = 900;

	auto window = std::make_unique<WindowFrame>(VGText("Vanguard"), defaultWindowSizeX, defaultWindowSizeY);
	window->onFocusChanged = &OnFocusChanged;
	window->onSizeChanged = &OnSizeChanged;

#if BUILD_DEBUG
	constexpr auto enableDebugging = true;
#else
	constexpr auto enableDebugging = false;
#endif

	auto device = std::make_unique<RenderDevice>(static_cast<HWND>(window->GetHandle()), false, enableDebugging);
	device->SetResolution(defaultWindowSizeX, defaultWindowSizeY, false);

	Renderer::Get().Initialize(std::move(window), std::move(device));

	// The input requires the user interface to be created first.
	Input::Initialize(Renderer::Get().window->GetHandle());
}

void EngineLoop()
{
	entt::registry tempReg;

	TransformComponent spectatorTransform{};
	spectatorTransform.translation.x = -10.f;
	spectatorTransform.translation.y = 0.f;
	spectatorTransform.translation.z = 1.f;

	const auto spectator = tempReg.create();
	tempReg.emplace<NameComponent>(spectator, "Spectator");
	tempReg.emplace<TransformComponent>(spectator, std::move(spectatorTransform));
	tempReg.emplace<CameraComponent>(spectator);
	tempReg.emplace<ControlComponent>(spectator);  // #TEMP

	TransformComponent sponzaTransform{};
	sponzaTransform.translation = { 1200.f, -50.f, -120.f };
	sponzaTransform.rotation = { -1.57079633f, 0.f, 0.f };  // Rotate the sponza into our coordinate space.

	const auto sponza = tempReg.create();
	tempReg.emplace<NameComponent>(sponza, "Sponza");
	tempReg.emplace<TransformComponent>(sponza, std::move(sponzaTransform));
	tempReg.emplace<MeshComponent>(sponza, AssetLoader::LoadMesh(*Renderer::Get().device, Config::shadersPath / "../Assets/Models/Sponza/glTF/Sponza.gltf"));

	LightComponent pointLight1{};
	pointLight1.color = { 1.f, 1.f, 1.f };
	LightComponent pointLight2{};
	pointLight2.color = { 1.f, 1.f, 1.f };
	LightComponent pointLight3{};
	pointLight3.color = { 1.f, 1.f, 1.f };

	TransformComponent light1Transform{};
	light1Transform.translation = { 800.f, 0.f, 70.f };
	TransformComponent light2Transform{};
	light2Transform.translation = { 1200.f, 0.f, 70.f };
	TransformComponent light3Transform{};
	light3Transform.translation = { 1600.f, 0.f, 70.f };

	const auto light1 = tempReg.create();
	tempReg.emplace<NameComponent>(light1, "Light1");
	tempReg.emplace<TransformComponent>(light1, std::move(light1Transform));
	tempReg.emplace<LightComponent>(light1, pointLight1);
	
	const auto light2 = tempReg.create();
	tempReg.emplace<NameComponent>(light2, "Light2");
	tempReg.emplace<TransformComponent>(light2, std::move(light2Transform));
	tempReg.emplace<LightComponent>(light2, pointLight2);

	const auto light3 = tempReg.create();
	tempReg.emplace<NameComponent>(light3, "Light3");
	tempReg.emplace<TransformComponent>(light3, std::move(light3Transform));
	tempReg.emplace<LightComponent>(light3, pointLight3);

	auto frameBegin = std::chrono::high_resolution_clock::now();
	float lastDeltaTime = 0.f;

	while (true)
	{
		{
			VGScopedCPUStat("Window Message Processing");

			MSG message{};
			if (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
			{
				::TranslateMessage(&message);
				::DispatchMessage(&message);
			}

			if (message.message == WM_QUIT)
			{
				return;
			}
		}

		ControlSystem::Update(tempReg);

		CameraSystem::Update(tempReg, lastDeltaTime);

		Renderer::Get().Render(tempReg);

		Renderer::Get().device->AdvanceCPU();

		auto frameEnd = std::chrono::high_resolution_clock::now();
		const auto frameDelta = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameBegin).count();
		frameBegin = frameEnd;
		lastDeltaTime = static_cast<float>(frameDelta) / 1000000.f;

		Renderer::Get().SubmitFrameTime(frameDelta);
		Input::SubmitFrameTime(frameDelta);
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