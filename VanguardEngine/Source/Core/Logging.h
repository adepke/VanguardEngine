// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Pragma.h>
#include <Core/Misc.h>

#if !BUILD_RELEASE
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
#if !BUILD_RELEASE
	inline tracy::Color::ColorType ResolveScopeColor(const char* Filename)
	{
		const std::filesystem::path Path{ Filename };
		const auto GenericWidePath{ Path.generic_wstring() };
		auto StartIndex = GenericWidePath.find(VGText("VanguardEngine/Source/"), 3);  // Assume we have at least the root drive letter in the path.
		if (StartIndex == std::wstring::npos)
		{
			return tracy::Color::Gray;  // Couldn't determine the module, default to gray.
		}

		StartIndex += 22;

		const auto EndIndex = GenericWidePath.find(VGText("/"), StartIndex + 3);  // Assume the module name is at least 3 characters long.
		if (EndIndex == std::wstring::npos)
		{
			return tracy::Color::Gray;  // Couldn't determine the module, default to gray.
		}

		const auto ModuleName = GenericWidePath.substr(StartIndex, EndIndex - StartIndex);

		if (ModuleName == VGText("Core"))
			return tracy::Color::Red;
		else if (ModuleName == VGText("Debug"))
			return tracy::Color::Green;
		else if (ModuleName == VGText("Editor"))
			return tracy::Color::Blue;
		else if (ModuleName == VGText("Rendering"))
			return tracy::Color::Orange;
		else if (ModuleName == VGText("Threading"))
			return tracy::Color::Purple;
		else if (ModuleName == VGText("Utility"))
			return tracy::Color::Aqua;
		else if (ModuleName == VGText("Window"))
			return tracy::Color::Cyan;
		else
			return tracy::Color::Gray;
	}
#endif

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
VGWarningDisable(4100, unused-parameter)
	// Can't use a stream operator here, causes precedence issues.
	void operator+=(const Detail::LogRecord& Record)
VGWarningPop
	{
#if !BUILD_RELEASE
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

#if BUILD_RELEASE
#define _Detail_VGLogBranch if constexpr (true) { do {} while (0); } else
#define _Detail_VGLogBranchFatal if constexpr (true) { VGBreak(); } else
#else
#define _Detail_VGLogBranch
#define _Detail_VGLogBranchFatal
#endif

#define _Detail_VGLogBase(Subsystem, Severity) LogManager::Get() += Detail::LogRecord{ Detail::SubsysToString(Subsystem), Severity }
#define _Detail_VGLogIntermediate(Subsystem, Severity) _Detail_VGLogBranch _Detail_VGLogBase(Subsystem, Severity)
#define _Detail_VGLogIntermediateFatal(Subsystem, Severity) _Detail_VGLogBranchFatal _Detail_VGLogBase(Subsystem, Severity)

#define VGLog(Subsystem) _Detail_VGLogIntermediate(Subsystem, Detail::LogSeverity::Log)
#define VGLogWarning(Subsystem) _Detail_VGLogIntermediate(Subsystem, Detail::LogSeverity::Warning)
#define VGLogError(Subsystem) _Detail_VGLogIntermediate(Subsystem, Detail::LogSeverity::Error)
#define VGLogFatal(Subsystem) _Detail_VGLogIntermediateFatal(Subsystem, Detail::LogSeverity::Fatal)

#if !BUILD_RELEASE
#define VGScopedCPUStat(Name) ZoneScopedNC(Name, static_cast<uint32_t>(Detail::ResolveScopeColor(__FILE__)))
#define VGScopedGPUStat(Name) do {} while (0)  // #TODO: GPU Profiling through PIX. Do we need the command list, command queue, and/or device context?
#define VGStatFrame FrameMark
#else
#define VGScopedCPUStat(Name) do {} while (0)
#define VGScopedGPUStat(Name) do {} while (0)
#define VGStatFrame do {} while (0)
#endif

// Global Subsystems

#define VGDeclareLogSubsystem(Name) struct ___##Name {}; static ___##Name Name; namespace Detail { constexpr const char* SubsysToString(___##Name) { return #Name; } }

VGDeclareLogSubsystem(Core);
VGDeclareLogSubsystem(Rendering);
VGDeclareLogSubsystem(Threading);
VGDeclareLogSubsystem(Utility);
VGDeclareLogSubsystem(Window);