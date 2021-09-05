// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Jobs/FiberRoutines.h>
#include <Jobs/Platform.h>

#include <atomic>  // std::atomic_flag
#include <cstddef>  // std::size_t
#include <memory>  // std::shared_ptr
#include <limits>  // std::numeric_limits

namespace Jobs
{
	class Manager;
	class FiberMutex;

	class Fiber
	{
		friend void ManagerFiberEntry(void*);

		using EntryType = void(*)(void*);

	private:
		void* context = nullptr;
		void* stack = nullptr;
		void* data = nullptr;

	public:
		bool waitPoolPriority = false;  // Used for alternating wait pool. Does not need to be atomic.
		size_t previousFiberIndex = std::numeric_limits<size_t>::max();  // Used to track the fiber that scheduled us.
		bool needsWaitEnqueue = false;  // Used to mark if we need to have availability restored or added to the wait pool.

		FiberMutex* mutex = nullptr;  // Used to determine if we're waiting on a mutex.

	public:
		Fiber() = default;
		Fiber(size_t stackSize, EntryType entry, Manager* owner);
		Fiber(const Fiber&) = delete;
		Fiber(Fiber&& other) noexcept;
		~Fiber();

		Fiber& operator=(const Fiber&) = delete;
		Fiber& operator=(Fiber&& other) noexcept;

		void Schedule(Fiber& from);

		void Swap(Fiber& other) noexcept;
	};
}