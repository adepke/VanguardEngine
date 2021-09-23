// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Core/Misc.h>

#include <spdlog/spdlog.h>

#include <string>
#include <atomic>

void RegisterCrashHandlers();
void ReportCrashEvent(const std::wstring& reason, bool printToLog);
bool HasDebuggerAttached();

// Force inline so the debugger breaks on the offending line, not in here.
VGForceInline void RequestCrash(const std::wstring& reason, bool printToLog = false)
{
	// Set when a fatal error is hit, used to give any present handling a chance to complete instead of racing.
	static std::atomic_flag crashed;

	// Only execute crash handling for the first reported instance. Once a thread reports a fatal error,
	// it is almost certainly going to cascade to other threads which will attempt to request a crash on their own.
	if (!crashed.test_and_set(std::memory_order::acquire))
	{
		ReportCrashEvent(reason, printToLog);

		spdlog::shutdown();  // Flushes all sinks.

		if (HasDebuggerAttached())
		{
			VGBreak();
		}

		// Use exit instead of std::terminate to avoid a message box.
		// #TODO: Consider proper thread shutdown instead of hard killing the process.
		exit(-1);
	}

	else
	{
		exit(0);  // Wasn't the first thread to crash, so just terminate.
	}
}