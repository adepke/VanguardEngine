// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <atomic>  // std::atomic_flag
#include <thread>  // std::this_thread

namespace Jobs
{
	class Spinlock
	{
	private:
		std::atomic_flag status = ATOMIC_FLAG_INIT;

	public:
		Spinlock() = default;
		Spinlock(const Spinlock&) = delete;
		Spinlock(Spinlock&&) noexcept = delete;

		Spinlock& operator=(const Spinlock&) = delete;
		Spinlock& operator=(Spinlock&&) noexcept = delete;

		inline void Lock();
		inline bool TryLock();
		inline void Unlock();
	};

	void Spinlock::Lock()
	{
		while (status.test_and_set(std::memory_order_acquire))[[unlikely]]
		{
			std::this_thread::yield();
		}
	}

	bool Spinlock::TryLock()
	{
		return !status.test_and_set(std::memory_order_acquire);
	}

	void Spinlock::Unlock()
	{
		status.clear(std::memory_order_release);
	}
}