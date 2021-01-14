// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Core/Pragma.h>
#include <Core/Misc.h>
#include <Utility/Singleton.h>

#if ENABLE_PROFILING
#include <Tracy.hpp>

// TracyD3D12.hpp includes wrl/client.h, which is a big leak.
// #TODO: Fix Windows.h leaking.
#define NOMINMAX

#include <TracyD3D12.hpp>
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

	constexpr auto* SeverityToString(LogSeverity severity)
	{
		switch (severity)
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

		const char* subsystem;
		const LogSeverity severity;
		std::chrono::time_point<LogClock> time = LogClock::now();
		std::wstringstream messageStream;

		LogRecord(const char* inSubsystem, LogSeverity inSeverity) : subsystem(inSubsystem), severity(inSeverity) {}

#if PLATFORM_WINDOWS
		LogRecord& operator<<(HRESULT hResult)
		{
			const auto narrowMessage{ std::system_category().message(hResult) };
			messageStream << std::wstring{ narrowMessage.begin(), narrowMessage.end() };
			return *this;
		}
#endif

		template <typename T>
		LogRecord& operator<<(const T& data)
		{
			messageStream << data;
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
	std::vector<std::unique_ptr<LogOutputBase>> outputs;
#endif

public:
	template <typename T>
	void AddOutput()
	{
#if ENABLE_LOGGING
		outputs.push_back(std::make_unique<T>());
#endif
	}

VGWarningPush
VGWarningDisable(4100, unused-parameter)
	// Can't use a stream operator here, causes precedence issues.
	void operator+=(const Detail::LogRecord& record)
VGWarningPop
	{
#if ENABLE_LOGGING
		for (auto& output : outputs)
		{
			output->Write(record);
		}

		if (record.severity == Detail::LogSeverity::Fatal) VGUnlikely
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

#define _Detail_VGLogBase(subsystem, severity) Logger::Get() += Detail::LogRecord{ Detail::SubsysToString(subsystem), severity }
#define _Detail_VGLogIntermediate(subsystem, severity) _Detail_VGLogBranch _Detail_VGLogBase(subsystem, severity)
#define _Detail_VGLogIntermediateFatal(subsystem, severity) _Detail_VGLogBranchFatal _Detail_VGLogBase(subsystem, severity)

#define VGLog(subsystem) _Detail_VGLogIntermediate(subsystem, Detail::LogSeverity::Log)
#define VGLogWarning(subsystem) _Detail_VGLogIntermediate(subsystem, Detail::LogSeverity::Warning)
#define VGLogError(subsystem) _Detail_VGLogIntermediate(subsystem, Detail::LogSeverity::Error)
#define VGLogFatal(subsystem) _Detail_VGLogIntermediateFatal(subsystem, Detail::LogSeverity::Fatal)

#if ENABLE_PROFILING
#define VGScopedCPUStat(name) ZoneScopedN(name)
#define VGScopedGPUStat(name, context, list) TracyD3D12Zone(context, list, name)
#define VGStatFrameCPU() FrameMark
#define VGStatFrameGPU(context) TracyD3D12Collect(context); TracyD3D12NewFrame(context)
#else
#define VGScopedCPUStat(name) do {} while (0)
#define VGScopedGPUStat(name) do {} while (0)
#define VGStatFrameCPU() do {} while (0)
#define VGStatFrameGPU(context) do {} while (0)
#endif

// Global Subsystems

#define VGDeclareLogSubsystem(name) struct ___##name {}; static ___##name name; namespace Detail { constexpr const char* SubsysToString(___##name) { return #name; } }

VGDeclareLogSubsystem(Asset);
VGDeclareLogSubsystem(Core);
VGDeclareLogSubsystem(Editor);
VGDeclareLogSubsystem(Rendering);
VGDeclareLogSubsystem(Threading);
VGDeclareLogSubsystem(Utility);
VGDeclareLogSubsystem(Window);