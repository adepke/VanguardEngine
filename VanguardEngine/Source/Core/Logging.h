// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Core/Pragma.h>
#include <Core/Misc.h>
#include <Utility/Singleton.h>
#include <Core/CrashHandler.h>

#include <spdlog/logger.h>

#if ENABLE_PROFILING
#include <Tracy.hpp>

// TracyD3D12.hpp includes wrl/client.h, which is a big leak.
// #TODO: Fix Windows.h leaking.
#define NOMINMAX

#include <TracyD3D12.hpp>
#include <WinPixEventRuntime/pix3.h>
#endif

#include <memory>

// #TODO: Stringify errors, see: fmt::report_windows_error and fmt::windows_error.
inline HRESULT GetPlatformError() { return GetLastError(); }

#define _Detail_VGFormatExtract(format, ...) VGText(format), __VA_ARGS__

#define VGLog(logger, ...) SPDLOG_LOGGER_INFO(logger, _Detail_VGFormatExtract(__VA_ARGS__))
#define VGLogWarning(logger, ...) SPDLOG_LOGGER_WARN(logger, _Detail_VGFormatExtract(__VA_ARGS__))
#define VGLogError(logger, ...) SPDLOG_LOGGER_ERROR(logger, _Detail_VGFormatExtract(__VA_ARGS__))
#if ENABLE_LOGGING
#define VGLogCritical(logger, ...) SPDLOG_LOGGER_CRITICAL(logger, _Detail_VGFormatExtract(__VA_ARGS__)); RequestCrash();
#else
#define VGLogCritical(logger, ...) RequestCrashMessage(VGText("Enable logging for more information."));
#endif

extern std::shared_ptr<spdlog::logger> logAsset;
extern std::shared_ptr<spdlog::logger> logCore;
extern std::shared_ptr<spdlog::logger> logRendering;
extern std::shared_ptr<spdlog::logger> logThreading;
extern std::shared_ptr<spdlog::logger> logUtility;
extern std::shared_ptr<spdlog::logger> logWindow;

#if ENABLE_PROFILING
#define VGScopedCPUStat(name) ZoneScopedN(name)
#define VGScopedCPUTransientStat(name) ZoneTransientN(__transientCpuZone, name, true)
#define VGScopedGPUStat(name, context, list) TracyD3D12Zone(context, list, name); PIXScopedEvent(list, PIX_COLOR_DEFAULT, name)
#define VGScopedGPUTransientStat(name, context, list) TracyD3D12ZoneTransient(context, __transientGpuZone, list, name, true); PIXScopedEvent(list, PIX_COLOR_DEFAULT, name)
#define VGStatFrameCPU() FrameMark
#define VGStatFrameGPU(context) TracyD3D12Collect(context); TracyD3D12NewFrame(context)
#else
#define VGScopedCPUStat(name) do {} while (0)
#define VGScopedGPUStat(name) do {} while (0)
#define VGStatFrameCPU() do {} while (0)
#define VGStatFrameGPU(context) do {} while (0)
#endif