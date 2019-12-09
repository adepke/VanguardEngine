// Copyright (c) 2019 Andrew Depke

#pragma once

namespace Jobs
{
#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS 1
	constexpr auto SizeOfUserSpaceLock = 40;
	constexpr auto SizeOfConditionVariable = 8;
#else
#define PLATFORM_POSIX 1
	constexpr auto SizeOfUserSpaceLock = 40;
	constexpr auto SizeOfConditionVariable = 48;
#endif
}

#ifndef PLATFORM_WINDOWS
#define PLATFORM_WINDOWS 0
#endif
#ifndef PLATFORM_POSIX
#define PLATFORM_POSIX 0
#endif

namespace Jobs
{
	class FutexConditionVariable
	{
	private:
		// Opaque housing.
		unsigned char UserSpaceLock[SizeOfUserSpaceLock];
		unsigned char ConditionVariable[SizeOfConditionVariable];

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