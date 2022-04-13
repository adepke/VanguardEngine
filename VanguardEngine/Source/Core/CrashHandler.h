// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Core/Misc.h>

#include <spdlog/spdlog.h>

#include <mutex>
#include <condition_variable>

void RegisterCrashHandlers();
void ReportInternalCrashEvent(const std::wstring& reason, bool printToLog);
bool HasDebuggerAttached();

// Used to immediate crashing, such as a critical log or assert statement.
// Force inline so the debugger breaks on the offending line, not in here.
VGForceInline void RequestCrash(const std::function<void()>& handler = []() {})
{
	// Flag is set when a fatal error is hit, used to give any crash handling a chance to complete instead of racing.
	static bool crashedFlag = false;
	static std::mutex mutex;
	static std::condition_variable condVar;

	std::unique_lock lock{ mutex };

	// Only execute crash handling for the first reported instance. Once a thread reports a fatal error,
	// it is almost certainly going to cascade to other threads which will attempt to request a crash on their own.
	if (!crashedFlag)
	{
		crashedFlag = true;
		lock.unlock();

		// We were the first thread to report a crash, execute our handler.
		handler();

		spdlog::shutdown();  // Flushes all sinks.

		if (HasDebuggerAttached())
		{
			VGBreak();
		}

		condVar.notify_all();

		// Use exit instead of std::terminate to avoid a message box.
		// #TODO: Consider proper thread shutdown instead of hard killing the process.
		exit(-1);
	}

	else
	{
		condVar.wait(lock);

		// We weren't the first thread to crash, and the crashing thread has finished instrumentation or reporting,
		// so exit out.

		exit(0);
	}
}

VGForceInline void RequestCrashMessage(const std::wstring& reason)
{
	RequestCrash([&reason]()
	{
		ReportInternalCrashEvent(reason, true);
	});
}