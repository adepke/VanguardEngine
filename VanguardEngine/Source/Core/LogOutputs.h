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

#if ENABLE_PROFILING
#include <Tracy.hpp>
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

#if ENABLE_PROFILING

struct LogProfilerOutput : LogOutputBase
{
	CriticalSection Lock;

	virtual ~LogProfilerOutput() = default;

	virtual void Write(const Detail::LogRecord& Out) override
	{
		std::wstringstream Stream{};
		Stream << "[" << Out.Subsystem << "." << Detail::SeverityToString(Out.Severity) << "] " << Out.MessageStream.str() << std::endl;

		const auto WideMessageString{ Stream.str() };
		const std::string MessageString{ WideMessageString.cbegin(), WideMessageString.cend() };  // Unsafe conversion, should probably put a safer system in place.

		switch (Out.Severity)
		{
		case Detail::LogSeverity::Warning: TracyMessageC(MessageString.c_str(), MessageString.size(), tracy::Color::Yellow); break;
		case Detail::LogSeverity::Error: TracyMessageC(MessageString.c_str(), MessageString.size(), tracy::Color::Red); break;
		case Detail::LogSeverity::Fatal: TracyMessageC(MessageString.c_str(), MessageString.size(), tracy::Color::Red); break;
		default: TracyMessage(MessageString.c_str(), MessageString.size()); break;
		}
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