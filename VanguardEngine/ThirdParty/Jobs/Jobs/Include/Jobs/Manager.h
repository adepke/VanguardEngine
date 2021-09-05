// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Jobs/Worker.h>
#include <Jobs/Fiber.h>
#include <Jobs/Counter.h>
#include <Jobs/Profiling.h>

#include <vector>  // std::vector
#include <array>  // std::array
#include <utility>  // std::move, std::pair
#include <thread>  // std::thread
#include <variant>  // std::variant
#include <mutex>  // std::mutex
#include <string>  // std::string
#include <memory>  // std::shared_ptr, std::unique_ptr
#include <map>  // std::map
#include <type_traits>  // std::is_same, std::decay
#include <optional>  // std::optional
#include <new>  // std::hardware_destructive_interference_size

namespace Jobs
{
	namespace Detail
	{
#ifdef __cpp_lib_hardware_interference_size
		constexpr auto hardwareDestructiveInterference = std::hardware_destructive_interference_size;
#else
		constexpr auto hardwareDestructiveInterference = 64;
#endif
	}

	class Manager
	{
		friend class Worker;
		friend class FiberMutex;
		friend void ManagerWorkerEntry(void*);
		friend void ManagerFiberEntry(void*);

		// #TODO: Move these into template traits.
		static constexpr size_t fiberCount = 256;
		static constexpr size_t fiberStackSize = 64 * 1024;  // 64 kB

	private:
		std::vector<Worker> workers;
		std::array<std::pair<Fiber, std::atomic_bool>, fiberCount> fibers;  // Pool of fibers paired to an availability flag.
		moodycamel::ConcurrentQueue<size_t> waitingFibers;  // Queue of fiber indices that are waiting for some dependency or scheduled a waiting fiber.

		static constexpr auto invalidID = std::numeric_limits<size_t>::max();

		std::atomic_bool ready;
		alignas(Detail::hardwareDestructiveInterference) std::atomic_bool shutdown;

		// Used to cycle the worker thread to enqueue in.
		std::atomic_uint enqueueIndex;

		alignas(Detail::hardwareDestructiveInterference) FutexConditionVariable queueCV;

		// #TODO: Use a more efficient hash map data structure.
		std::map<std::string, std::shared_ptr<Counter<>>> groupMap;

		template <typename U>
		void EnqueueInternal(U&& job);

	public:
		Manager() = default;
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = delete;  // #TODO: Implement.
		~Manager();

		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = delete;  // #TODO: Implement.

		void Initialize(size_t threadCount = 0);

		template <typename U>
		void Enqueue(U&& job);

		template <size_t Size>
		void Enqueue(Job (&jobs)[Size]);

		template <typename U>
		void Enqueue(U&& job, const std::shared_ptr<Counter<>>& counter);

		template <size_t Size>
		void Enqueue(Job (&jobs)[Size], const std::shared_ptr<Counter<>>& counter);

		template <typename U>
		std::shared_ptr<Counter<>> Enqueue(U&& job, const std::string& group);

		template <size_t Size>
		std::shared_ptr<Counter<>> Enqueue(Job (&jobs)[Size], const std::string& group);

		size_t GetWorkerCount() const { return workers.size(); }

	private:
		std::optional<JobBuilder> Dequeue(size_t threadID);

		size_t GetThisThreadID() const;
		inline bool IsValidID(size_t id) const;

		inline bool CanContinue() const;

		size_t GetAvailableFiber();  // Returns a fiber that is not currently scheduled.
	};

	template <typename U>
	void Manager::EnqueueInternal(U&& job)
	{
		JOBS_SCOPED_STAT("Enqueue Internal");

		// If we're a job builder, we need to increment the counter before leaving Enqueue.
		if (job.stream)
		{
			job.GetCounter().operator++();  // We need to increment the counter since we might end up waiting on it immediately, before the sub jobs are enqueued. Decremented in the job builder.
		}

		auto thisThreadID{ GetThisThreadID() };

		if (IsValidID(thisThreadID))
		{
			workers[thisThreadID].GetJobQueue().enqueue(std::forward<JobBuilder>(static_cast<JobBuilder&&>(job)));
		}

		else
		{
			auto cachedEI{ enqueueIndex.load(std::memory_order_acquire) };

			// Note: We might lose an increment here if this runs in parallel, but we would rather suffer that instead of locking.
			enqueueIndex.store((cachedEI + 1) % workers.size(), std::memory_order_release);

			workers[cachedEI].GetJobQueue().enqueue(std::forward<JobBuilder>(static_cast<JobBuilder&&>(job)));
		}
	}

	template <typename U>
	void Manager::Enqueue(U&& job)
	{
		if constexpr (!std::is_same_v<std::decay_t<U>, Job> && !std::is_same_v<std::decay_t<U>, JobBuilder>)
		{
			static_assert(false, "Enqueue only supports objects of type Job");
		}

		else
		{
			EnqueueInternal(std::forward<JobBuilder>(static_cast<JobBuilder&&>(job)));

			JOBS_SCOPED_STAT("Enqueue Notify");

			// #NOTE: Safeguarding the notify can destroy performance in high enqueue situations. This leaves a blind spot potential,
			// but the risk is worth it. Even if a blind spot signal happens, the worker will just sleep until a new enqueue arrives, where it can recover.
			//queueCV.Lock();
			queueCV.NotifyOne();  // Notify one sleeper. They will work steal if they don't get the job enqueued directly.
			//queueCV.Unlock();
		}
	}

	template <size_t Size>
	void Manager::Enqueue(Job (&jobs)[Size])
	{
		for (auto iter = 0; iter < Size; ++iter)
		{
			EnqueueInternal(jobs[iter]);
		}

		queueCV.NotifyAll();  // Notify all sleepers.
	}

	template <typename U>
	void Manager::Enqueue(U&& job, const std::shared_ptr<Counter<>>& counter)
	{
		if constexpr (std::is_same_v<std::decay_t<U>, JobBuilder>)
		{
			static_assert(false, "Cannot enqueue a JobBuilder with a custom counter");
		}

		else if constexpr (!std::is_same_v<std::decay_t<U>, Job>)
		{
			static_assert(false, "Enqueue only supports objects of type Job");
		}

		else
		{
			counter->operator++();
			job.atomicCounter = counter;

			Enqueue(job);
		}
	}

	template <size_t Size>
	void Manager::Enqueue(Job (&jobs)[Size], const std::shared_ptr<Counter<>>& counter)
	{
		counter->operator+=(Size);

		for (auto iter = 0; iter < Size; ++iter)
		{
			jobs[iter].atomicCounter = counter;
		}

		Enqueue(jobs);
	}

	template <typename U>
	std::shared_ptr<Counter<>> Manager::Enqueue(U&& job, const std::string& group)
	{
		if constexpr (!std::is_same_v<std::decay_t<U>, Job> && !std::is_same_v<std::decay_t<U>, JobBuilder>)
		{
			static_assert(false, "Enqueue only supports objects of type Job");
		}

		else
		{
			std::shared_ptr<Counter<>> groupCounter;

			auto allocateCounter{ []() { return std::make_shared<Counter<>>(1); } };

			if (group.empty())
			{
				groupCounter = std::move(allocateCounter());
			}

			else
			{
				auto groupIter{ groupMap.find(group) };

				if (groupIter != groupMap.end())
				{
					groupCounter = groupIter->second;
					groupCounter->operator++();
				}

				else
				{
					groupCounter = std::move(allocateCounter());
					groupMap.insert({ group, groupCounter });
				}
			}

			job.atomicCounter = groupCounter;
			Enqueue(job);

			return groupCounter;
		}
	}

	template <size_t Size>
	std::shared_ptr<Counter<>> Manager::Enqueue(Job (&jobs)[Size], const std::string& group)
	{
		std::shared_ptr<Counter<>> groupCounter;

		auto allocateCounter{ []() { return std::make_shared<Counter<>>(1); } };

		if (group.empty())
		{
			groupCounter = std::move(allocateCounter());
		}

		else
		{
			auto groupIter{ groupMap.find(group) };

			if (groupIter != groupMap.end())
			{
				groupCounter = groupIter->second;
				groupCounter->operator+=(Size);
			}

			else
			{
				groupCounter = std::move(allocateCounter());
				groupMap.insert({ group, groupCounter });
			}
		}

		for (auto iter = 0; iter < Size; ++iter)
		{
			jobs[iter].atomicCounter = groupCounter;
		}

		Enqueue(jobs);

		return groupCounter;
	}

	bool Manager::IsValidID(size_t id) const
	{
		return id != invalidID;
	}

	bool Manager::CanContinue() const
	{
		return !shutdown.load(std::memory_order_acquire);
	}
}