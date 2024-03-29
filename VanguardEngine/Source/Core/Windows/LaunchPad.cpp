// Copyright (c) 2019-2022 Andrew Depke

#include <Core/Engine.h>
#include <Core/Globals.h>
//#include <Core/Windows/WindowsMinimal.h>  // We actually can't include this since we need some API that's excluded from lean and mean.

#define NOMINMAX
#include <Windows.h>

#include <Core/Logging.h>  // spdlog leaks Windows.h, likely with lean and mean defined, so we have to include it after.

#include <thread>

void ParseCommandLine()
{
	VGScopedCPUStat("Parse Command Line");

	int count = 0;
	auto** argV = ::CommandLineToArgvW(::GetCommandLine(), &count);

	for (int i = 0; i < count; ++i)
	{
		GCommandLineArgs.push_back(argV[i]);
	}

	::LocalFree(argV);
}

VGWarningPush
VGWarningDisable(4100, unused-parameter)
int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
VGWarningPop
{
	ParseCommandLine();

	GProcessThreads.emplace_back(std::this_thread::get_id());

	return static_cast<int>(EngineMain());
}