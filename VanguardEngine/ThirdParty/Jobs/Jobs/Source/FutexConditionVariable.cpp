// Copyright (c) 2019 Andrew Depke

#include <Jobs/FutexConditionVariable.h>

#if PLATFORM_WINDOWS
#include <Jobs/WindowsMinimal.h>
static_assert(sizeof(CRITICAL_SECTION) == Jobs::SizeOfUserSpaceLock, "Invalid CRITICAL_SECTION size! Header requires update.");
static_assert(sizeof(CONDITION_VARIABLE) == Jobs::SizeOfConditionVariable, "Invalid CONDITION_VARIABLE size! Header requires update.");
#else
#include <pthread.h>
static_assert(sizeof(pthread_mutex_t) == Jobs::SizeOfUserSpaceLock, "Invalid pthread_mutex_t size! Header requires update.");
static_assert(sizeof(pthread_cond_t) == Jobs::SizeOfConditionVariable, "Invalid pthread_cond_t size! Header requires update.");
#endif

namespace Jobs
{
	FutexConditionVariable::FutexConditionVariable()
	{
#if PLATFORM_WINDOWS
		InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(UserSpaceLock));
		SetCriticalSectionSpinCount(reinterpret_cast<CRITICAL_SECTION*>(UserSpaceLock), 2000);

		InitializeConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(ConditionVariable));
#else
		*reinterpret_cast<pthread_mutex_t*>(UserSpaceLock) = PTHREAD_MUTEX_INITIALIZER;
		*reinterpret_cast<pthread_cond_t*>(ConditionVariable) = PTHREAD_COND_INITIALIZER;
#endif
	}

	FutexConditionVariable::~FutexConditionVariable()
	{
#if PLATFORM_WINDOWS
		DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(UserSpaceLock));
#else
		pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(UserSpaceLock));
		pthread_cond_destroy(reinterpret_cast<pthread_cond_t*>(ConditionVariable));
#endif
	}

	void FutexConditionVariable::Lock()
	{
#if PLATFORM_WINDOWS
		EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(UserSpaceLock));
#else
#endif
	}

	void FutexConditionVariable::Unlock()
	{
#if PLATFORM_WINDOWS
		LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(UserSpaceLock));
#else
#endif
	}

	void FutexConditionVariable::Wait()
	{
#if PLATFORM_WINDOWS
		SleepConditionVariableCS(reinterpret_cast<CONDITION_VARIABLE*>(ConditionVariable), reinterpret_cast<CRITICAL_SECTION*>(UserSpaceLock), INFINITE);
#else
#endif
	}

	void FutexConditionVariable::NotifyOne()
	{
#if PLATFORM_WINDOWS
		WakeConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(ConditionVariable));
#else
#endif
	}

	void FutexConditionVariable::NotifyAll()
	{
#if PLATFORM_WINDOWS
		WakeAllConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(ConditionVariable));
#else
#endif
	}
}