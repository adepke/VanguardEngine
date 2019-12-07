// Copyright (c) 2019 Andrew Depke

#pragma once

#if PLATFORM_WINDOWS
constexpr auto SizeOfHandle = 40;
#else
constexpr auto SizeOfHandle = 40;
#endif

struct CriticalSection
{
private:
	unsigned char Handle[SizeOfHandle];

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