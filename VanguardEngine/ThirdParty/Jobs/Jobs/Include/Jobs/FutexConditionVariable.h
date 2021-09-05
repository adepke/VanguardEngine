// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Jobs/Platform.h>

namespace Jobs
{
#if JOBS_PLATFORM_WINDOWS
	constexpr auto sizeOfUserSpaceLock = 40;
	constexpr auto sizeOfConditionVariable = 8;
#endif
#if JOBS_PLATFORM_POSIX
	constexpr auto sizeOfUserSpaceLock = 40;
	constexpr auto sizeOfConditionVariable = 48;
#endif
}

namespace Jobs
{
	class FutexConditionVariable
	{
	private:
		// Opaque storage.
		unsigned char userSpaceLock[sizeOfUserSpaceLock];
		unsigned char conditionVariable[sizeOfConditionVariable];

	public:
		FutexConditionVariable();
		FutexConditionVariable(const FutexConditionVariable&) = delete;
		FutexConditionVariable(FutexConditionVariable&&) noexcept = delete;
		~FutexConditionVariable();

		FutexConditionVariable& operator=(const FutexConditionVariable&) = delete;
		FutexConditionVariable& operator=(FutexConditionVariable&&) noexcept = delete;

		void Lock();
		void Unlock();

		void Wait();
		void NotifyOne();
		void NotifyAll();
	};
}