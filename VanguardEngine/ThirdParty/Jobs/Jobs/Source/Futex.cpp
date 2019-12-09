// Copyright (c) 2019 Andrew Depke

#include <Jobs/Futex.h>

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS 1
#include <Jobs/WindowsMinimal.h>
#else
#define PLATFORM_POSIX 1
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <sys/time.h>
#include <limits>
#endif

#ifndef PLATFORM_WINDOWS
#define PLATFORM_WINDOWS 0
#endif
#ifndef PLATFORM_POSIX
#define PLATFORM_POSIX 0
#endif

namespace Jobs
{
	bool Futex::Wait(void* CompareAddress, size_t Size, uint64_t TimeoutNs) const
	{
#if PLATFORM_WINDOWS
		return WaitOnAddress(Address, CompareAddress, Size, TimeoutNs > 0 ? (TimeoutNs >= 1e6 ? TimeoutNs : 1e6) / 1e6 : INFINITE);
#else
		timespec Timeout;
		Timeout.tv_sec = static_cast<time_t>(TimeoutNs / (uint64_t)1e9);  // Whole seconds.
		Timeout.tv_nsec = static_cast<long>(TimeoutNs % (uint64_t)1e9);  // Remaining nano seconds.

		syscall(SYS_futex, CompareAddress, FUTEX_WAIT, &Address, TimeoutNs > 0 ? &Timeout : nullptr, nullptr, 0);
#endif
	}

	void Futex::NotifyOne() const
	{
		if (Address)
		{
#if PLATFORM_WINDOWS
			WakeByAddressSingle(Address);
#else
			syscall(SYS_futex, Address, FUTEX_WAKE, 1);
#endif
		}
	}

	void Futex::NotifyAll() const
	{
		if (Address)
		{
#if PLATFORM_WINDOWS
			WakeByAddressAll(Address);
#else
			syscall(SYS_futex, Address, FUTEX_WAKE, std::numeric_limits<int>::max());
#endif
		}
	}
}