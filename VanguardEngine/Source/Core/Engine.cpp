// Copyright (c) 2019-2022 Andrew Depke

#include <Core/Engine.h>
#include <Core/Base.h>
#include <Core/Config.h>
#include <Core/ConsoleVariable.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Window/WindowFrame.h>
#include <Core/Input.h>
#include <Core/CoreComponents.h>
#include <Core/CoreSystems.h>
#include <Core/CrashHandler.h>
#include <Core/LogSinks.h>
#include <Core/CommandLine.h>
#include <Scene/SceneManager.h>
#include <Editor/Editor.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include <string>
#include <memory>
#include <chrono>
#include <thread>

// #TEMP
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Asset/AssetLoader.h>
#include <Utility/Random.h>
#include <Asset/AssetManager.h>
#include <Asset/AssetComponents.h>
//

entt::registry registry;

void OnFocusChanged(bool focus)
{
	VGScopedCPUStat("Focus Changed");

	VGLog(logWindow, "{}", (focus ? VGText("Acquired focus.") : VGText("Released focus.")));

	// #TODO: Limit render FPS, disable audio.
}

void OnSizeChanged(uint32_t width, uint32_t height)
{
	VGScopedCPUStat("Size Changed");

	VGLog(logWindow, "Render size changed ({}, {}).", width, height);
	Renderer::Get().SetResolution(width, height, false);
}

// #TODO: consider moving to windows core?
bool LoadPIXLibrary()
{
	// Sample taken from: https://devblogs.microsoft.com/pix/taking-a-capture/

	// Early out if already loaded.
	if (GetModuleHandle(L"WinPixGpuCapturer.dll") != 0)
	{
		return true;
	}

	LPWSTR programFilesPath = nullptr;
	SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

	std::filesystem::path pixInstallationPath = programFilesPath;
	pixInstallationPath /= "Microsoft PIX";

	std::wstring newestVersionFound;

	for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath))
	{
		if (directory_entry.is_directory())
		{
			if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str())
			{
				newestVersionFound = directory_entry.path().filename().c_str();
			}
		}
	}

	if (newestVersionFound.empty())
	{
		return false;
	}

	return LoadLibrary((pixInstallationPath / newestVersionFound / VGText("WinPixGpuCapturer.dll")).c_str()) != nullptr;
}

// This function doesn't belong here, refactor.
void SetupDefaultScene()
{
	// Atmosphere and weather.
	const auto atmosphere = registry.create();
	registry.emplace<NameComponent>(atmosphere, "Atmosphere");
	registry.emplace<WeatherComponent>(atmosphere, WeatherComponent{
		.coverage = 0.5f,
		.precipitation = 0.3f,
		.windStrength = 0.2f,
		.windDirection = {1.f, 0.f}
	});

	const auto AddHelmet = [](const TransformComponent& transform)
	{
		const auto path = Config::shadersPath / "../Assets/Models/DamagedHelmet/HelmetTangents.glb";
		const auto entity = registry.create();
		registry.emplace<NameComponent>(entity, "Helmet");
		registry.emplace<TransformComponent>(entity, transform);
		registry.emplace<AssetComponent>(entity, path);
		registry.emplace<MeshComponent>(entity, AssetManager::Get().LoadModel(path));

		return entity;
	};

	const auto AddSponza = [](const TransformComponent& transform)
	{
		const auto path = Config::shadersPath / "../Assets/Models/Sponza/glTF/Sponza.gltf";
		const auto entity = registry.create();
		registry.emplace<NameComponent>(entity, "Sponza");
		registry.emplace<TransformComponent>(entity, transform);
		registry.emplace<AssetComponent>(entity, path);
		registry.emplace<MeshComponent>(entity, AssetManager::Get().LoadModel(path));

		return entity;
	};

	const auto AddBistro = [](const TransformComponent& transform)
	{
		const auto path = Config::shadersPath / "../Assets/Models/Bistro/Bistro2.gltf";
		const auto entity = registry.create();
		registry.emplace<NameComponent>(entity, "Bistro");
		registry.emplace<TransformComponent>(entity, transform);
		registry.emplace<AssetComponent>(entity, path);
		registry.emplace<MeshComponent>(entity, AssetManager::Get().LoadModel(path));

		return entity;
	};

	const auto AddTerrain = []()
	{
		TransformComponent transform{};
		transform.translation = { 780.f, -1260.f, 0.f };
		transform.rotation = { -90 * 3.14159f / 180.f, 0.f, 5.5 * 3.14159f / 180.f };
		transform.scale = { 70.f, 70.f, 70.f };

		const auto path = Config::shadersPath / "../Assets/Models/deathValley.glb";
		const auto entity = registry.create();
		registry.emplace<NameComponent>(entity, "Terrain");
		registry.emplace<TransformComponent>(entity, transform);
		registry.emplace<AssetComponent>(entity, path);
		registry.emplace<MeshComponent>(entity, AssetManager::Get().LoadModel(path));

		return entity;
	};

	const auto AddSanMiguel = []()
	{
		TransformComponent transform{};

		const auto path = Config::shadersPath / "../Assets/Models/SanMiguel.glb";
		const auto entity = registry.create();
		registry.emplace<NameComponent>(entity, "San Miguel");
		registry.emplace<TransformComponent>(entity, transform);
		registry.emplace<AssetComponent>(entity, path);
		registry.emplace<MeshComponent>(entity, AssetManager::Get().LoadModel(path));

		return entity;
	};

	const auto AddSphere = []()
	{
		TransformComponent transform{};

		const auto path = Config::shadersPath / "../Assets/Models/sphere.glb";
		const auto entity = registry.create();
		registry.emplace<NameComponent>(entity, "Sphere");
		registry.emplace<TransformComponent>(entity, transform);
		registry.emplace<AssetComponent>(entity, path);
		registry.emplace<MeshComponent>(entity, AssetManager::Get().LoadModel(path));

		return entity;
	};

	//AddSanMiguel();

	AddTerrain();

	//AddSphere();

	/*AddHelmet({
		.scale = { 100.f, 100.f, 100.f },
		.rotation = { -169.5f * 3.14159f / 180.f, 0.f, 121.5f * 3.14159f / 180.f },
		.translation = { 78.f, 0.f, -5.f }
	});*/

	TransformComponent spectatorTransform{};
	spectatorTransform.translation = { 0.f, 0.f, 238.f };
	spectatorTransform.rotation = { 0.f, 0.f, 0.f };

	const auto spectator = registry.create();
	registry.emplace<NameComponent>(spectator, "Spectator");
	registry.emplace<TransformComponent>(spectator, std::move(spectatorTransform));
	registry.emplace<CameraComponent>(spectator);

	/*const auto h = AssetManager::Get().LoadModel(Config::shadersPath / "../Assets/Models/DamagedHelmet/HelmetTangents.glb");
	for (int i = 0; i < 6; i++)
	{
		float s = i*i * 19.f + 5;
		TransformComponent transform = {
			.scale = { s,s,s },
			.rotation = { -169.5f * 3.14159f / 180.f, 0.f, 121.5f * 3.14159f / 180.f },
			//.rotation = {0.f,0.f,0.f},
			.translation = { i*i*i * 73.f + 40.f * i, i*i * -160.f, 100.f}
		};

		const auto entity = registry.create();
		registry.emplace<TransformComponent>(entity, transform);
		registry.emplace<MeshComponent>(entity, h);
	}*/
	//
	//AddHelmet({
	//	.scale = { 10.f, 10.f, 10.f },
	//	.rotation = { 169.5f * 3.14159f / 180.f, 0.f, -20.5f * 3.14159f / 180.f },
	//	.translation = { 50.f, -20.f, 8.f }
	//});

	//AddSponza({
	//	.scale = { 1.f, 1.f, 1.f },
	//	.rotation = { -90.f * 3.14159f / 180.f, 0.f, 0.f },
	//	//.translation = { 120.f, -3.f, -21.f }
	//	.translation = { 120.f, -3.f, -3500.f }
	//});

	const auto helmetPath = Config::shadersPath / "../Assets/Models/DamagedHelmet/HelmetTangents.glb";
	const auto helmetMesh = AssetManager::Get().LoadModel(helmetPath);
	int perAxis = 0;
	float spacing = 50.f;
	for (int i = 0; i < perAxis; i++)
	{
		for (int j = 0; j < perAxis; j++)
		{
			for (int k = 0; k < perAxis; k++)
			{
				TransformComponent transform = {
					.scale = { 10.f, 10.f, 10.f },
					.rotation = { (float)Rand(-2.0, 2.0) * 3.142f, (float)Rand(-2.0, 2.0) * 3.142f, (float)Rand(-2.0, 2.0) * 3.142f },
					.translation = {
						(i * spacing) - (perAxis * spacing / 2.f),
						(j * spacing) - (perAxis * spacing / 2.f),
						(k * spacing) - (perAxis * spacing / 2.f)
					}
				};

				const auto entity = registry.create();
				registry.emplace<TransformComponent>(entity, transform);
				registry.emplace<AssetComponent>(entity, helmetPath);
				registry.emplace<MeshComponent>(entity, helmetMesh);
			}
		}
	}

	const auto light = registry.create();
	registry.emplace<LightComponent>(light, LightComponent{ .type = LightType::Point, .color = { 1.f, 1.f, 1.f } });
	registry.emplace<TransformComponent>(light, TransformComponent{ .scale = { 1.f, 1.f, 1.f }, .rotation = { 0.f, 0.f, 0.f }, .translation = { -15.f, 28.f, 3200.f } });

	int lightCount = 0;//10000;
	//int lightCount = 20000;

	for (int i = 0; i < lightCount; ++i)
	{
		LightComponent pointLight{ .type = LightType::Point, .color = { (float)Rand(0.2f, 1.f), (float)Rand(0.2f, 1.f), (float)Rand(0.2f, 1.f) } };
		// Sponza lights.
		TransformComponent transform{
			.scale = { 1.f, 1.f, 1.f },
			.rotation = { 0.f, 0.f, 0.f },
			.translation = { (float)Rand(-150.0, 150.0), (float)Rand(-65.0, 65.0), (float)Rand(0.0, 120.0) }
		};

		// Sun temple lights.
		//TransformComponent transform{
		//	.scale = { 1.f, 1.f, 1.f },
		//	.rotation = { 0.f, 0.f, 0.f },
		//	.translation = { (float)Rand(-150.0, 150.0), (float)Rand(-700, 700), (float)Rand(20, 100) }
		//};

		const auto light = registry.create();
		registry.emplace<LightComponent>(light, pointLight);
		registry.emplace<TransformComponent>(light, transform);
	}
}

bool EngineBoot()
{
	VGScopedCPUStat("Engine Boot");

	auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Log.txt", true);
	auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
	auto tracySink = std::make_shared<TracySink_mt>();
	auto editorSink = std::make_shared<EditorSink_mt>();

	logCore = std::make_shared<spdlog::logger>("core", spdlog::sinks_init_list{ fileSink, msvcSink, tracySink, editorSink });
	logAsset = logCore->clone("asset");
	logEditor = logCore->clone("editor");
	logRendering = logCore->clone("rendering");
	logScene = logCore->clone("scene");
	logThreading = logCore->clone("threading");
	logUtility = logCore->clone("utility");
	logWindow = logCore->clone("window");

	spdlog::set_default_logger(logCore);
	spdlog::set_pattern("[%H:%M:%S.%e][tid:%t][%n.%l] %v");
	spdlog::flush_on(spdlog::level::err);
	spdlog::flush_every(1s);

	// Not useful to set an error handler, this isn't invoked unless exceptions are enabled.
	// With exceptions disabled, spdlog just writes to stderr.
	// #TODO: Consider changing the behavior of error handling with exceptions disabled.
	//spdlog::set_error_handler([](const std::string& msg)
	//{
	//	VGLogError(logCore, "Logger: {}", msg);
	//});

	Config::Initialize();

	if (!GCommandLineOptions.valid)
	{
		VGLogError(logCore, "Invalid command line arguments. Expected: [--headless] [--output <file.png>] [--delay <frames>] [--scene <file>] [--pix] [--profile] [--cvar <name=value>]");
		return false;
	}

	if (GCommandLineOptions.pix && !LoadPIXLibrary())
	{
		VGLogError(logCore, "Failed to load PIX library");
		return false;
	}

	Input::EnableDPIAwareness();

	constexpr uint32_t defaultWindowSizeX = 1920;
	constexpr uint32_t defaultWindowSizeY = 1080;

	const bool windowVisible = !GCommandLineOptions.headless;
	auto window = std::make_unique<WindowFrame>(VGText("Vanguard"), defaultWindowSizeX, defaultWindowSizeY, windowVisible);
	window->onFocusChanged = &OnFocusChanged;
	window->onSizeChanged = &OnSizeChanged;

	auto enableDebugging = false;
#if BUILD_DEBUG || BUILD_DEVELOPMENT
	// No need for debugging in headless
	if (!GCommandLineOptions.headless)
	{
		enableDebugging = true;
	}
#endif

	auto device = std::make_unique<RenderDevice>(static_cast<HWND>(window->GetHandle()), false, enableDebugging);
	Renderer::Get().Initialize(std::move(window), std::move(device), registry);

	// The input requires the user interface to be created first.
	Input::Initialize(Renderer::Get().window->GetHandle());

	// Disable editor in headless mode.
	if (GCommandLineOptions.headless)
	{
		Editor::Get().enabled = false;
	}

	AssetManager::Get().Initialize(Renderer::Get().device.get());

	// Early out if any registered cvars are invalid. This isn't bulletproof, but it can help catch bad cvars early.
	if (CvarManager::Get().HasFailedOverride())
	{
		VGLogError(logCore, "One or more command line console variable overrides were invalid.");
		return false;
	}

	// Check if a specific scene has been requested, otherwise go to the default.
	if (GCommandLineOptions.scene.has_value())
	{
		std::filesystem::path scenePath = *GCommandLineOptions.scene;
		if (scenePath.is_relative())
		{
			scenePath = Config::scenesPath / scenePath;
		}

		if (!Scene::Load(registry, scenePath))
		{
			return false;
		}
	}

	else
	{
		SetupDefaultScene();
	}

	if (registry.view<const CameraComponent>().size() == 0)
	{
		VGLogWarning(logScene, "No camera found in the scene.");
	}

	return true;
}

// #TODO: refactor.
bool HandleMessages()
{
	VGScopedCPUStat("Window Message Processing");

	MSG message{};
	while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
	{
		::TranslateMessage(&message);
		::DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			return true;
		}
	}

	return false;
}

bool EngineLoop()
{
	// Headless mode runs a slightly different loop.
	if (GCommandLineOptions.headless)
	{
		const auto& output = *GCommandLineOptions.output;
		const uint32_t captureFrame = GCommandLineOptions.delayFrames;
		constexpr float fixedDelta = 1.f / 60.f;  // Make temporal effects consistent.

		for (uint32_t frame = 0; ; ++frame)
		{
			bool quit = HandleMessages();
			if (quit)
			{
				// Failed to capture.
				return false;
			}

			// #TODO: Core systems needs to be refactored out, this is the same in headless and interactive.

			AssetManager::Get().Update();

			// Note: skip ControlSystem, this isn't interactive.
			CameraSystem::Update(registry, fixedDelta);
			TimeOfDaySystem::Update(registry, fixedDelta);

			// Request at the beginning of rendering.
			if (frame == captureFrame)
			{
				Renderer::Get().RequestCapture(output);
			}

			Renderer::Get().Render(registry);
			Renderer::Get().device->AdvanceCPU();

			if (frame == captureFrame)
			{
				break;
			}
		}

		return Renderer::Get().CaptureSucceeded();
	}

	auto frameBegin = std::chrono::high_resolution_clock::now();
	float lastDeltaTime = 0.f;

	while (true)
	{
		bool quit = HandleMessages();
		if (quit)
		{
			break;
		}

		AssetManager::Get().Update();

		ControlSystem::Update(registry);
		CameraSystem::Update(registry, lastDeltaTime);
		TimeOfDaySystem::Update(registry, lastDeltaTime);

		Renderer::Get().Render(registry);

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

	VGLog(logCore, "Engine shutting down.");
}


// Wait for Tracy to connect, and ensure it drains all data before exit.
void DrainProfiler()
{
#if ENABLE_PROFILING
	// Wait for Tracy to connect, in case it hasn't started yet. All trace data is buffered in memory
	// until the server drains it.
	const auto deadline = std::chrono::steady_clock::now() + 10s;
	while (!TracyIsConnected && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(10ms);
	}

	if (!TracyIsConnected)
	{
		VGLogWarning(logCore, "--profile was requested, but server was not connected at shutdown time. Potential trace data loss.");
		return;
	}

	tracy::GetProfiler().RequestShutdown();
	while (!tracy::GetProfiler().HasShutdownFinished())
	{
		std::this_thread::sleep_for(10ms);
	}
#else
	VGLogError(logCore, "--profile was requested, but engine was not compiled with profiling enabled.");
#endif
}

int32_t EngineMain()
{
	RegisterCrashHandlers();

	auto healthy = true;
	healthy = EngineBoot();
	if (!healthy)
		return 1;
	healthy = EngineLoop();

	if (GCommandLineOptions.profile)
	{
		DrainProfiler();
	}

	if (!healthy)
		return 1;
	EngineShutdown();

	return 0;
}