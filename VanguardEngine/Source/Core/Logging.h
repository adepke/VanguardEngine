// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Pragma.h>
#include <Core/Misc.h>
#include <Utility/Singleton.h>

#if ENABLE_PROFILING
#include <Tracy.hpp>
#endif

#include <chrono>
#include <vector>
#include <sstream>
#include <memory>
#include <system_error>
#include <filesystem>
#include <cstring>

#if PLATFORM_WINDOWS && !defined(HRESULT)
using HRESULT = long;
#endif

// Can't use auto type deduction if the definition is in a source file, so we need to do this instead.
#if PLATFORM_WINDOWS
using PlatformErrorType = HRESULT;
#else
using PlatformErrorType = int32_t;
#endif

PlatformErrorType GetPlatformError();

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

class Logger : public Singleton<Logger>
{
#if ENABLE_LOGGING
private:
	std::vector<std::unique_ptr<LogOutputBase>> Outputs;
#endif

public:
	template <typename T>
	void AddOutput()
	{
#if ENABLE_LOGGING
		Outputs.push_back(std::make_unique<T>());
#endif
	}

VGWarningPush
VGWarningDisable(4100, unused-parameter)
	// Can't use a stream operator here, causes precedence issues.
	void operator+=(const Detail::LogRecord& Record)
VGWarningPop
	{
#if ENABLE_LOGGING
		for (auto& Output : Outputs)
		{
			Output->Write(Record);
		}

		if (Record.Severity == Detail::LogSeverity::Fatal) VGUnlikely
		{
			VGBreak();
		}
#endif
	}
};

#if !ENABLE_LOGGING
#define _Detail_VGLogBranch if constexpr (true) { do {} while (0); } else
#define _Detail_VGLogBranchFatal if constexpr (true) { VGBreak(); } else
#else
#define _Detail_VGLogBranch
#define _Detail_VGLogBranchFatal
#endif

#define _Detail_VGLogBase(Subsystem, Severity) Logger::Get() += Detail::LogRecord{ Detail::SubsysToString(Subsystem), Severity }
#define _Detail_VGLogIntermediate(Subsystem, Severity) _Detail_VGLogBranch _Detail_VGLogBase(Subsystem, Severity)
#define _Detail_VGLogIntermediateFatal(Subsystem, Severity) _Detail_VGLogBranchFatal _Detail_VGLogBase(Subsystem, Severity)

#define VGLog(Subsystem) _Detail_VGLogIntermediate(Subsystem, Detail::LogSeverity::Log)
#define VGLogWarning(Subsystem) _Detail_VGLogIntermediate(Subsystem, Detail::LogSeverity::Warning)
#define VGLogError(Subsystem) _Detail_VGLogIntermediate(Subsystem, Detail::LogSeverity::Error)
#define VGLogFatal(Subsystem) _Detail_VGLogIntermediateFatal(Subsystem, Detail::LogSeverity::Fatal)

#if ENABLE_PROFILING
#define VGScopedCPUStat(Name) ZoneScopedN(Name)
#define VGScopedGPUStat(Name) do {} while (0)  // #TODO: GPU Profiling through PIX. Do we need the command list, command queue, and/or device context?
#define VGStatFrame FrameMark
#else
#define VGScopedCPUStat(Name) do {} while (0)
#define VGScopedGPUStat(Name) do {} while (0)
#define VGStatFrame do {} while (0)
#endif

// Global Subsystems

#define VGDeclareLogSubsystem(Name) struct ___##Name {}; static ___##Name Name; namespace Detail { constexpr const char* SubsysToString(___##Name) { return #Name; } }

VGDeclareLogSubsystem(Asset);
VGDeclareLogSubsystem(Core);
VGDeclareLogSubsystem(Editor);
VGDeclareLogSubsystem(Rendering);
VGDeclareLogSubsystem(Threading);
VGDeclareLogSubsystem(Utility);
VGDeclareLogSubsystem(Window);