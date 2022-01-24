// Copyright (c) 2019-2022 Andrew Depke

#include <Threading/CriticalSection.h>

#include <pthread.h>

static_assert(sizeof(pthread_mutex_t) == SizeOfHandle, "Invalid pthread_mutex_t size! Header requires update.");

CriticalSection::CriticalSection()
{
	// PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP may be slightly slower.
	*reinterpret_cast<pthread_mutex_t*>(Handle) = PTHREAD_MUTEX_INITIALIZER;  //PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
}

CriticalSection::~CriticalSection()
{
	pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(Handle));
}

void CriticalSection::lock()
{
	pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(Handle));
}

bool CriticalSection::try_lock()
{
	return pthread_mutex_trylock(reinterpret_cast<pthread_mutex_t*>(Handle)) == 0;
}

void CriticalSection::unlock() noexcept
{
	pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(Handle));
}