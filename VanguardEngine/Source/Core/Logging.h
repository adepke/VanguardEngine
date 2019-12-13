// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Core/Pragma.h>

#if !BUILD_RELEASE
#include <Tracy.hpp>
#endif

#include <chrono>
#include <vector>
#include <sstream>
#include <memory>
#include <system_error>

#if PLATFORM_WINDOWS && !defined(HRESULT)
using HRESULT = long;
#endif

auto GetPlatformError();

namespace Detail
{
	enum class LogSeverity
	{
		Log,
		Warning,
		Error,
		Fatal
	};

	constexpr auto* SeverityToString(LogSeverity In)
	{
		switch (In)
		{
		case LogSeverity::Log:
			return "Log";
		case LogSeverity::Warning:
			return "Warning";
		case LogSeverity::Error:
			return "Error";
		case LogSeverity::Fatal:
			return "Fatal";
		}

		return "";
	}

	struct LogRecord
	{
		using LogClock = std::chrono::system_clock;

		const char* Subsystem;
		const LogSeverity Severity;
		std::chrono::time_point<LogClock> Time = LogClock::now();
		std::wstringstream MessageStream;

		LogRecord(const char* InSubsystem, LogSeverity InSeverity) : Subsystem(InSubsystem), Severity(InSeverity) {}

#if PLATFORM_WINDOWS
		LogRecord& operator<<(HRESULT In)
		{
			const auto NarrowMessage{ std::system_category().message(In) };
			MessageStream << std::wstring{ NarrowMessage.begin(), NarrowMessage.end() };
			return *this;
		}
#endif

		template <typename T>
		LogRecord& operator<<(const T& In)
		{
			MessageStream << In;
			return *this;
		}
	};
}

struct LogOutputBase
{
	virtual ~LogOutputBase() = default;
	virtual void Write(const Detail::LogRecord&) = 0;
};

class LogManager
{
#if !BUILD_RELEASE
private:
	std::vector<std::unique_ptr<LogOutputBase>> Outputs;
#endif

public:
	LogManager() = default;
	LogManager(const LogManager&) = delete;
	LogManager(LogManager&&) noexcept = delete;
	LogManager& operator=(const LogManager&) = delete;
	LogManager& operator=(LogManager&&) noexcept = delete;

	static inline LogManager& Get()
	{
		static LogManager This;
		return This;
	}

	template <typename T>
	void AddOutput()
	{
#if !BUILD_RELEASE
		Outputs.push_back(std::make_unique<T>());
#endif
	}

VGWarningPush
VGWarningDisable(4100, "-Wunused-parameter")
	// Can't use a stream operator here, causes precedence issues.
	void operator+=(const Detail::LogRecord& Record)
VGWarningPop
	{
#if !BUILD_RELEASE
		for (auto& Output : Outputs)
		{
			Output->Write(Record);
		}
#endif
	}
};

#if BUILD_RELEASE
#define _Detail_VGLogBranch if constexpr (true) { do {} while (0); } else
#else
#define _Detail_VGLogBranch
#endif

#define _Detail_VGLogBase(Subsystem, Severity) _Detail_VGLogBranch LogManager::Get() += Detail::LogRecord{ Detail::SubsysToString(Subsystem), Severity }

#define VGLog(Subsystem) _Detail_VGLogBase(Subsystem, Detail::LogSeverity::Log)
#define VGLogWarning(Subsystem) _Detail_VGLogBase(Subsystem, Detail::LogSeverity::Warning)
#define VGLogError(Subsystem) _Detail_VGLogBase(Subsystem, Detail::LogSeverity::Error)
#define VGLogFatal(Subsystem) _Detail_VGLogBase(Subsystem, Detail::LogSeverity::Fatal)

#if !BUILD_RELEASE
#define VGScopedCPUStat(Name) ZoneScopedN(Name)
#define VGScopedGPUStat(Name) do {} while (0)  // #TODO: GPU Profiling.
#define VGStatFrame FrameMark
#else
#define VGScopedCPUStat(Name) do {} while (0)
#define VGScopedGPUStat(Name) do {} while (0)
#define VGStatFrame do {} while (0)
#endif

// Global Subsystems

#define VGDeclareLogSubsystem(Name) struct _##Name {}; _##Name Name; namespace Detail { constexpr const char* SubsysToString(_##Name) { return #Name; } }

VGDeclareLogSubsystem(Core);
VGDeclareLogSubsystem(Renderer);
VGDeclareLogSubsystem(Threading);
VGDeclareLogSubsystem(Utility);