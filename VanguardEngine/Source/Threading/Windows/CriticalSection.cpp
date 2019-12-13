// Copyright (c) 2019 Andrew Depke

#include <Threading/CriticalSection.h>
#include <Core/Windows/WindowsMinimal.h>

static_assert(sizeof(CRITICAL_SECTION) == SizeOfHandle, "Invalid CRITICAL_SECTION size! Header requires update.");

CriticalSection::CriticalSection()
{
	InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(Handle));
	SetCriticalSectionSpinCount(reinterpret_cast<CRITICAL_SECTION*>(Handle), 2000);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(Handle));
}

void CriticalSection::lock()
{
	EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(Handle));
}

bool CriticalSection::try_lock()
{
	return TryEnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(Handle));
}

void CriticalSection::unlock() noexcept
{
	LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(Handle));
}