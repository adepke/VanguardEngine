// Copyright (c) 2019-2021 Andrew Depke

#include <Threading/CriticalSection.h>
#include <Core/Windows/WindowsMinimal.h>

static_assert(sizeof(CRITICAL_SECTION) == sizeOfHandle, "Invalid CRITICAL_SECTION size! Header requires update.");

CriticalSection::CriticalSection()
{
	InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(handle));
	SetCriticalSectionSpinCount(reinterpret_cast<CRITICAL_SECTION*>(handle), 2000);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(handle));
}

void CriticalSection::lock()
{
	EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(handle));
}

bool CriticalSection::try_lock()
{
	return TryEnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(handle));
}

void CriticalSection::unlock() noexcept
{
	LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(handle));
}