// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#if ENABLE_LOGGING

#include <Threading/CriticalSection.h>

#include <sstream>
#include <mutex>
#include <fstream>

#if PLATFORM_WINDOWS
#include <Core/Windows/WindowsMinimal.h>
#endif

struct LogFileOutput : LogOutputBase
{
	CriticalSection Lock;
	std::wofstream FileStream;

	// #TODO: Fetch log file from config.
	LogFileOutput() : FileStream("Log.txt") {}
	virtual ~LogFileOutput() = default;

	virtual void Write(const Detail::LogRecord& Out) override
	{
		std::wstringstream Stream{};
		Stream << "[" << Out.Subsystem << "." << Detail::SeverityToString(Out.Severity) << "] " << Out.MessageStream.str() << std::endl;

		std::lock_guard Guard{ Lock };

		FileStream << Stream.str();
	}
};

#if PLATFORM_WINDOWS

struct LogWindowsOutput : LogOutputBase
{
	CriticalSection Lock;

	virtual ~LogWindowsOutput() = default;

	virtual void Write(const Detail::LogRecord& Out) override
	{
		std::wstringstream Stream{};
		Stream << "[" << Out.Subsystem << "." << Detail::SeverityToString(Out.Severity) << "] " << Out.MessageStream.str() << std::endl;

		std::lock_guard Guard{ Lock };

		OutputDebugString(Stream.str().c_str());
	}
};

#endif

#if PLATFORM_WINDOWS
using DefaultLogOutput = LogWindowsOutput;
#else
using DefaultLogOutput = LogFileOutput;
#endif

#else

struct LogNone;

using DefaultLogOutput = LogNone;

#endif