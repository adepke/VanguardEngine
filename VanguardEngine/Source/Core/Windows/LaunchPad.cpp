// Copyright (c) 2019-2020 Andrew Depke

#include <Core/Engine.h>
#include <Core/Globals.h>
#include <Core/Logging.h>
//#include <Core/Windows/WindowsMinimal.h>  // We actually can't include this since we need some API that's excluded from lean and mean.

#include <Windows.h>

void ParseCommandLine()
{
	VGScopedCPUStat("Parse Command Line");

	int Count = 0;
	auto** ArgV = ::CommandLineToArgvW(::GetCommandLine(), &Count);

	for (int Index = 0; Index < Count; ++Index)
	{
		GCommandLineArgs.push_back(ArgV[Index]);
	}

	::LocalFree(ArgV);
}

VGWarningPush
VGWarningDisable(4100, unused-parameter)
int WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
VGWarningPop
{
	ParseCommandLine();

	return static_cast<int>(EngineMain());
}