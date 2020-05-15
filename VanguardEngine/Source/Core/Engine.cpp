// Copyright (c) 2019-2020 Andrew Depke

#include <Core/Engine.h>
#include <Core/LogOutputs.h>
#include <Core/Config.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Window/WindowFrame.h>
#include <Core/InputManager.h>

#include <string>
#include <memory>
// #TEMP
#include <thread>
#include <Rendering/RenderComponents.h>
#include <Rendering/RenderSystems.h>
#include <Core/CoreComponents.h>

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

	WindowFrame::Get().Create(VGText("Vanguard Engine"), DefaultWindowSizeX, DefaultWindowSizeY);
	WindowFrame::Get().OnFocusChanged = &OnFocusChanged;
	WindowFrame::Get().OnSizeChanged = &OnSizeChanged;
	WindowFrame::Get().RestrainCursor(CursorRestraint::ToCenter);

#if BUILD_DEBUG
	constexpr auto EnableDebugging = true;
#else
	constexpr auto EnableDebugging = false;
#endif

	auto Device = std::make_unique<RenderDevice>(static_cast<HWND>(WindowFrame::Get().GetHandle()), false, EnableDebugging);
	Device->SetResolution(DefaultWindowSizeX, DefaultWindowSizeY, false);
	Renderer::Get().Initialize(std::move(Device));
}

void EngineLoop()
{
	entt::registry TempReg;

	TransformComponent SpectatorTransform{};
	SpectatorTransform.Translation.x = -10.f;
	SpectatorTransform.Translation.y = 0.f;
	SpectatorTransform.Translation.z = 1.f;

	CameraComponent SpectatorCamera{};

	const auto Spectator = TempReg.create();
	TempReg.assign<TransformComponent>(Spectator, std::move(SpectatorTransform));
	TempReg.assign<CameraComponent>(Spectator, std::move(SpectatorCamera));

	for (int Index = 0; Index < 1; ++Index)
	{
		Vertex v0{ { -1.f, -1.f, 1.f }, { 0.4f, 0.f, 0.f, 1.f } };
		Vertex v1{ { 1.f, -1.f, 1.f }, { 0.f, 0.4f, 0.f, 1.f } };
		Vertex v2{ { 1.f, 1.f, 1.f }, { 0.f, 0.f, 0.4f, 1.f } };
		Vertex v3{ { -1.f, 1.f, 1.f }, { 0.f, 0.2f, 0.f, 1.f } };

		Vertex v4{ { -1.f, -1.f, -1.f }, { 0.f, 0.4f, 0.f, 1.f } };
		Vertex v5{ { -1.f, 1.f, -1.f }, { 0.4f, 0.f, 0.f, 1.f } };
		Vertex v6{ { 1.f, 1.f, -1.f }, { 0.f, 0.f, 0.4f, 1.f } };
		Vertex v7{ { 1.f, -1.f, -1.f }, { 0.2f, 0.f, 0.f, 1.f } };

		Vertex v8{ { -1.f, 1.f, -1.f }, { 0.f, 0.4f, 0.f, 1.f } };
		Vertex v9{ { -1.f, 1.f, 1.f }, { 0.4f, 0.f, 0.f, 1.f } };
		Vertex v10{ { 1.f, 1.f, 1.f }, { 0.f, 0.f, 0.4f, 1.f } };
		Vertex v11{ { 1.f, 1.f, -1.f }, { 0.f, 0.2f, 0.f, 1.f } };

		Vertex v12{ { -1.f, -1.f, -1.f }, { 0.f, 0.f, 0.4f, 1.f } };
		Vertex v13{ { 1.f, -1.f, -1.f }, { 0.f, 0.4f, 0.f, 1.f } };
		Vertex v14{ { 1.f, -1.f, 1.f }, { 0.4f, 0.f, 0.f, 1.f } };
		Vertex v15{ { -1.f, -1.f, 1.f }, { 0.f, 0.f, 0.2f, 1.f } };

		Vertex v16{ { 1.f, -1.f, -1.f }, { 0.4f, 0.f, 0.f, 1.f } };
		Vertex v17{ { 1.f, 1.f, -1.f }, { 0.f, 0.f, 0.4f, 1.f } };
		Vertex v18{ { 1.f, 1.f, 1.f }, { 0.f, 0.4f, 0.f, 1.f } };
		Vertex v19{ { 1.f, -1.f, 1.f }, { 0.2f, 0.f, 0.f, 1.f } };

		Vertex v20{ { -1.f, -1.f, -1.f }, { 0.f, 0.4f, 0.f, 1.f } };
		Vertex v21{ { -1.f, -1.f, 1.f }, { 0.4f, 0.f, 0.f, 1.f } };
		Vertex v22{ { -1.f, 1.f, 1.f }, { 0.f, 0.f, 0.4f, 1.f } };
		Vertex v23{ { -1.f, 1.f, -1.f }, { 0.f, 0.2f, 0.f, 1.f } };

		auto Mesh{ CreateMeshComponent(*Renderer::Get().Device, std::vector<Vertex>{ v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15,v16,v17,v18,v19,v20,v21,v22,v23 }, std::vector<uint32_t>{ 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23 }) };

		const auto Entity = TempReg.create();
		TempReg.assign<TransformComponent>(Entity);
		TempReg.assign<MeshComponent>(Entity, std::move(Mesh));
	}

	while (true)
	{
		{
			VGScopedCPUStat("Window Message Processing");

			MSG Message{};
			if (::PeekMessage(&Message, static_cast<HWND>(WindowFrame::Get().GetHandle()), 0, 0, PM_REMOVE))
			{
				::TranslateMessage(&Message);
				::DispatchMessage(&Message);
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
}

int32_t EngineMain()
{
	EngineBoot();

	EngineLoop();

	EngineShutdown();

	return 0;
}