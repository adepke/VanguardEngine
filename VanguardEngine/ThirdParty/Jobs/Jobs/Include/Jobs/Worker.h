// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include "../../ThirdParty/ConcurrentQueue/concurrentqueue.h"
#include <Jobs/JobBuilder.h>

#include <cstddef>  // std::size_t
#include <thread>  // std::thread
#include <atomic>  // std::atomic

namespace Jobs
{
	class Manager;
	class Fiber;

	class Worker
	{
		using EntryType = void(*)(void*);

	private:
		Manager* owner = nullptr;
		std::thread threadHandle;
		size_t id;  // Manager-specific ID.

		Fiber* threadFiber = nullptr;
		moodycamel::ConcurrentQueue<JobBuilder> jobQueue;

		static constexpr size_t invalidFiberIndex = std::numeric_limits<size_t>::max();

	public:
		Worker(Manager* inOwner, size_t inID, EntryType entry);
		Worker(const Worker&) = delete;
		Worker(Worker&& other) noexcept { Swap(other); }
		~Worker();

		Worker& operator=(const Worker&) = delete;
		Worker& operator=(Worker&& other) noexcept
		{
			Swap(other);

			return *this;
		}

		size_t fiberIndex = invalidFiberIndex;  // Index into the owner's fiber pool that we're executing. Allows for fibers to become aware of their own ID.

		std::thread& GetHandle() { return threadHandle; }
		std::thread::id GetNativeID() const { return threadHandle.get_id(); }
		size_t GetID() const { return id; }

		Fiber& GetThreadFiber() const { return *threadFiber; }
		moodycamel::ConcurrentQueue<JobBuilder>& GetJobQueue() { return jobQueue; }

		constexpr bool IsValidFiberIndex(size_t index) const { return index != invalidFiberIndex; }

		void Swap(Worker& other) noexcept;
	};
}