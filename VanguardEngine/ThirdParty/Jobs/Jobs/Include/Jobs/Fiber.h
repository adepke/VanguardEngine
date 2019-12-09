// Copyright (c) 2019 Andrew Depke

#pragma once

#include <atomic>  // std::atomic_flag
#include <cstddef>  // std::size_t
#include <memory>  // std::shared_ptr
#include <limits>  // std::numeric_limits

namespace Jobs
{
	void FiberEntry(void* Data);

	class Fiber
	{
	private:
		void* Context = nullptr;
		void* Data = nullptr;

	public:
		bool WaitPoolPriority = false;  // Used for alternating wait pool. Does not need to be atomic.
		size_t PreviousFiberIndex = std::numeric_limits<size_t>::max();  // Used to track the fiber that scheduled us.
		bool NeedsWaitEnqueue = false;  // Used to mark if we need to have availability restored or added to the wait pool.

	public:
		Fiber() = default;  // Used for converting a thread to a fiber.
		Fiber(size_t StackSize, decltype(&FiberEntry) Entry, void* Arg);
		Fiber(const Fiber&) = delete;
		Fiber(Fiber&& Other) noexcept;
		~Fiber();

		Fiber& operator=(const Fiber&) = delete;
		Fiber& operator=(Fiber&& Other) noexcept;

		void Schedule(const Fiber& From);

		void Swap(Fiber& Other) noexcept;

		static Fiber* FromThisThread(void* Arg);
	};
}