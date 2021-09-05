// Copyright (c) 2019-2021 Andrew Depke

#include <Jobs/Futex.h>
#include <Jobs/Platform.h>
#include <Jobs/Profiling.h>

#if JOBS_PLATFORM_WINDOWS
  #include <Jobs/WindowsMinimal.h>
#endif
#if JOBS_PLATFORM_POSIX
  #include <unistd.h>
  #include <sys/syscall.h>
  #include <linux/futex.h>
  #include <sys/time.h>
  #include <limits>
#endif

namespace Jobs
{
	bool Futex::Wait(void* compareAddress, size_t size, uint64_t timeoutNs) const
	{
		JOBS_SCOPED_STAT("Futex Wait");

#if JOBS_PLATFORM_WINDOWS
		return WaitOnAddress(address, compareAddress, size, static_cast<DWORD>(timeoutNs > 0 ? (timeoutNs >= 1e6 ? timeoutNs : 1e6) / 1e6 : INFINITE));
#else
		timespec timeout;
		timeout.tv_sec = static_cast<time_t>(timeoutNs / (uint64_t)1e9);  // Whole seconds.
		timeout.tv_nsec = static_cast<long>(timeoutNs % (uint64_t)1e9);  // Remaining nano seconds.

		syscall(SYS_futex, compareAddress, FUTEX_WAIT, &address, timeoutNs > 0 ? &timeout : nullptr, nullptr, 0);
#endif
	}

	void Futex::NotifyOne() const
	{
		JOBS_SCOPED_STAT("Futex Notify One");

		if (address)
		{
#if JOBS_PLATFORM_WINDOWS
			WakeByAddressSingle(address);
#else
			syscall(SYS_futex, address, FUTEX_WAKE, 1);
#endif
		}
	}

	void Futex::NotifyAll() const
	{
		JOBS_SCOPED_STAT("Futex Notify All");

		if (address)
		{
#if JOBS_PLATFORM_WINDOWS
			WakeByAddressAll(address);
#else
			syscall(SYS_futex, address, FUTEX_WAKE, std::numeric_limits<int>::max());
#endif
		}
	}
}