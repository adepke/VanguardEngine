// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <ostream>

#include <Jobs/Spinlock.h>

#if JOBS_ENABLE_LOGGING
#define JOBS_LOG(level, format, ...) Jobs::LogManager::Get().Log(level, format, ## __VA_ARGS__)
#else
#define JOBS_LOG(level, format, ...) do {} while (0)
#endif

namespace Jobs
{
	enum class LogLevel
	{
		Log,
		Warning,
		Error
	};

	class LogManager
	{
	private:
		Spinlock lock;

		std::ostream* outputDevice = nullptr;

	public:
		LogManager() = default;
		LogManager(const LogManager&) = delete;
		LogManager(LogManager&&) noexcept = delete;
		LogManager& operator=(const LogManager&) = delete;
		LogManager& operator=(LogManager&&) noexcept = delete;

		static inline LogManager& Get()
		{
			static LogManager singleton;

			return singleton;
		}

		void Log(LogLevel level, const std::string& format, ...);

		void SetOutputDevice(std::ostream& device);
	};
}