// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#if PLATFORM_WINDOWS
constexpr auto sizeOfHandle = 40;
#else
constexpr auto sizeOfHandle = 40;
#endif

struct CriticalSection
{
private:
	unsigned char handle[sizeOfHandle];

public:
	CriticalSection();
	CriticalSection(const CriticalSection&) = delete;
	CriticalSection(CriticalSection&&) noexcept = delete;
	~CriticalSection();

	CriticalSection& operator=(const CriticalSection&) = delete;
	CriticalSection& operator=(CriticalSection&&) noexcept = delete;

	void lock();
	bool try_lock();
	void unlock() noexcept;
};