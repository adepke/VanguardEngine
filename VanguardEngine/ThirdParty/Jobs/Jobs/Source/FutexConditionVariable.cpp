// Copyright (c) 2019-2021 Andrew Depke

#include <Jobs/FutexConditionVariable.h>

#if JOBS_PLATFORM_WINDOWS
  #include <Jobs/WindowsMinimal.h>
  static_assert(sizeof(CRITICAL_SECTION) == Jobs::sizeOfUserSpaceLock, "Invalid CRITICAL_SECTION size! Header requires update.");
  static_assert(sizeof(CONDITION_VARIABLE) == Jobs::sizeOfConditionVariable, "Invalid CONDITION_VARIABLE size! Header requires update.");
#endif
#if JOBS_PLATFORM_POSIX
  #include <pthread.h>
  static_assert(sizeof(pthread_mutex_t) == Jobs::sizeOfUserSpaceLock, "Invalid pthread_mutex_t size! Header requires update.");
  static_assert(sizeof(pthread_cond_t) == Jobs::sizeOfConditionVariable, "Invalid pthread_cond_t size! Header requires update.");
#endif

namespace Jobs
{
	FutexConditionVariable::FutexConditionVariable()
	{
#if JOBS_PLATFORM_WINDOWS
		InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(userSpaceLock));
		SetCriticalSectionSpinCount(reinterpret_cast<CRITICAL_SECTION*>(userSpaceLock), 2000);

		InitializeConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(conditionVariable));
#else
		*reinterpret_cast<pthread_mutex_t*>(userSpaceLock) = PTHREAD_MUTEX_INITIALIZER;
		*reinterpret_cast<pthread_cond_t*>(conditionVariable) = PTHREAD_COND_INITIALIZER;
#endif
	}

	FutexConditionVariable::~FutexConditionVariable()
	{
#if JOBS_PLATFORM_WINDOWS
		DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(userSpaceLock));
#else
		pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(userSpaceLock));
		pthread_cond_destroy(reinterpret_cast<pthread_cond_t*>(conditionVariable));
#endif
	}

	void FutexConditionVariable::Lock()
	{
#if JOBS_PLATFORM_WINDOWS
		EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(userSpaceLock));
#else
#endif
	}

	void FutexConditionVariable::Unlock()
	{
#if JOBS_PLATFORM_WINDOWS
		LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(userSpaceLock));
#else
#endif
	}

	void FutexConditionVariable::Wait()
	{
#if JOBS_PLATFORM_WINDOWS
		SleepConditionVariableCS(reinterpret_cast<CONDITION_VARIABLE*>(conditionVariable), reinterpret_cast<CRITICAL_SECTION*>(userSpaceLock), INFINITE);
#else
#endif
	}

	void FutexConditionVariable::NotifyOne()
	{
#if JOBS_PLATFORM_WINDOWS
		WakeConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(conditionVariable));
#else
#endif
	}

	void FutexConditionVariable::NotifyAll()
	{
#if JOBS_PLATFORM_WINDOWS
		WakeAllConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(conditionVariable));
#else
#endif
	}
}