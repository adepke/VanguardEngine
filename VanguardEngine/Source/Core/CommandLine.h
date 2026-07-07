// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <filesystem>
#include <optional>
#include <cstdint>
#include <vector>
#include <string>

struct CommandLineCvarOverride
{
	std::wstring name;
	std::wstring value;
};

struct CommandLineOptions
{
	// Tracks if the options are valid or not.
	bool valid = false;

	// OPTIONS

	// --headless: render offscreen, capture a single frame and output to a file.
	bool headless = false;

	// --output <file>: destination path for the captured frame. Must end in ".png".
	std::optional<std::filesystem::path> output;

	// --delay <n>: number of accumulation frames to render before the capture frame.
	uint32_t delayFrames = 0;

	// --scene <file>: scene file to load. Relative or absolute.
	std::optional<std::filesystem::path> scene;

	// --pix: enables PIX DLL loading for capture.
	bool pix = false;

	// --cvar <name=value>: overrides a console variable.
	std::vector<CommandLineCvarOverride> cvarOverrides;
};

// Parses GCommandLineArgs into GCommandLineOptions. Safe to call before logging is
// initialized; failures are recorded in GCommandLineOptions.valid and reported later.
void ParseCommandLineOptions(const std::vector<std::wstring>& args);
