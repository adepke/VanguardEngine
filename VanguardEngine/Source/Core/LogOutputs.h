// Copyright (c) 2019-2021 Andrew Depke

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
	CriticalSection lock;
	std::wofstream fileStream;

	// #TODO: Fetch log file from config.
	LogFileOutput() : fileStream("Log.txt") {}
	virtual ~LogFileOutput() = default;

	virtual void Write(const Detail::LogRecord& out) override
	{
		std::wstringstream stream{};
		stream << "[" << out.subsystem << "." << Detail::SeverityToString(out.severity) << "] " << out.messageStream.str() << std::endl;

		std::scoped_lock lockScope{ lock };

		fileStream << stream.str();
	}
};

#if PLATFORM_WINDOWS

struct LogWindowsOutput : LogOutputBase
{
	CriticalSection lock;

	virtual ~LogWindowsOutput() = default;

	virtual void Write(const Detail::LogRecord& out) override
	{
		std::wstringstream stream{};
		stream << "[" << out.subsystem << "." << Detail::SeverityToString(out.severity) << "] " << out.messageStream.str() << std::endl;

		std::scoped_lock lockScope{ lock };

		OutputDebugString(stream.str().c_str());
	}
};

#endif

#if ENABLE_PROFILING

struct LogProfilerOutput : LogOutputBase
{
	CriticalSection lock;

	virtual ~LogProfilerOutput() = default;

	virtual void Write(const Detail::LogRecord& out) override
	{
		std::wstringstream stream{};
		stream << "[" << out.subsystem << "." << Detail::SeverityToString(out.severity) << "] " << out.messageStream.str() << std::endl;

		const auto wideMessageString{ stream.str() };
		const std::string messageString{ wideMessageString.cbegin(), wideMessageString.cend() };  // Unsafe conversion, should probably put a safer system in place.

		switch (out.severity)
		{
		case Detail::LogSeverity::Warning: TracyMessageC(messageString.c_str(), messageString.size(), tracy::Color::Yellow); break;
		case Detail::LogSeverity::Error: TracyMessageC(messageString.c_str(), messageString.size(), tracy::Color::Red); break;
		case Detail::LogSeverity::Fatal: TracyMessageC(messageString.c_str(), messageString.size(), tracy::Color::Red); break;
		default: TracyMessage(messageString.c_str(), messageString.size()); break;
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