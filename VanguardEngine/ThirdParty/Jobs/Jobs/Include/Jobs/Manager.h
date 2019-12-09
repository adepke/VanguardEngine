// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Jobs/Worker.h>
#include <Jobs/Fiber.h>
#include <Jobs/Counter.h>

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

namespace Jobs
{
	// Shared common data.
	struct FiberData
	{
		Manager* const Owner;

		FiberData(Manager* const InOwner) : Owner(InOwner) {};
	};

	class Manager
	{
		friend class Worker;
		friend void ManagerWorkerEntry(Manager* const Owner);
		friend void ManagerFiberEntry(void* Data);

		// #TODO: Move these into template traits.
		static constexpr size_t FiberCount = 64;
		static constexpr size_t FiberStackSize = 1024 * 1024;  // 1 MB

	private:
		// Shared fiber storage.
		std::unique_ptr<FiberData> Data;

		std::vector<Worker> Workers;
		std::array<std::pair<Fiber, std::atomic_bool>, FiberCount> Fibers;  // Pool of fibers paired to an availability flag.
		moodycamel::ConcurrentQueue<size_t> WaitingFibers;  // Queue of fiber indices that are waiting for some dependency or scheduled a waiting fiber.

		static constexpr auto InvalidID = std::numeric_limits<size_t>::max();

		std::atomic_bool Ready;
		alignas(std::hardware_destructive_interference_size) std::atomic_bool Shutdown;

		// Used to cycle the worker thread to enqueue in.
		std::atomic_uint EnqueueIndex;

		alignas(std::hardware_destructive_interference_size) FutexConditionVariable QueueCV;

		// #TODO: Use a more efficient hash map data structure.
		std::map<std::string, std::shared_ptr<Counter<>>> GroupMap;

		template <typename U>
		void EnqueueInternal(U&& InJob);

	public:
		Manager() = default;
		Manager(const Manager&) = delete;
		Manager(Manager&&) noexcept = delete;  // #TODO: Implement.
		~Manager();

		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = delete;  // #TODO: Implement.

		void Initialize(size_t ThreadCount = 0);

		template <typename U>
		void Enqueue(U&& InJob);

		template <size_t Size>
		void Enqueue(Job (&InJobs)[Size]);

		template <typename U>
		void Enqueue(U&& InJob, const std::shared_ptr<Counter<>>& InCounter);

		template <size_t Size>
		void Enqueue(Job (&InJobs)[Size], const std::shared_ptr<Counter<>>& InCounter);

		template <typename U>
		std::shared_ptr<Counter<>> Enqueue(U&& InJob, const std::string& Group);

		template <size_t Size>
		std::shared_ptr<Counter<>> Enqueue(Job (&InJobs)[Size], const std::string& Group);

		size_t GetWorkerCount() const { return Workers.size(); }

	private:
		std::optional<JobBuilder> Dequeue(size_t ThreadID);

		size_t GetThisThreadID() const;
		inline bool IsValidID(size_t ID) const;

		inline bool CanContinue() const;

		size_t GetAvailableFiber();  // Returns a fiber that is not currently scheduled.
	};

	template <typename U>
	void Manager::EnqueueInternal(U&& InJob)
	{
		// If we're a job builder, we need to increment the counter before leaving Enqueue.
		if (InJob.Stream)
		{
			InJob.GetCounter().operator++();  // We need to increment the counter since we might end up waiting on it immediately, before the sub jobs are enqueued. Decremented in the job builder.
		}

		auto ThisThreadID{ GetThisThreadID() };

		if (IsValidID(ThisThreadID))
		{
			Workers[ThisThreadID].GetJobQueue().enqueue(std::forward<JobBuilder>(static_cast<JobBuilder&&>(InJob)));
		}

		else
		{
			auto CachedEI{ EnqueueIndex.load(std::memory_order_acquire) };

			// Note: We might lose an increment here if this runs in parallel, but we would rather suffer that instead of locking.
			EnqueueIndex.store((CachedEI + 1) % Workers.size(), std::memory_order_release);

			Workers[CachedEI].GetJobQueue().enqueue(std::forward<JobBuilder>(static_cast<JobBuilder&&>(InJob)));
		}
	}

	template <typename U>
	void Manager::Enqueue(U&& InJob)
	{
		if constexpr (!std::is_same_v<std::decay_t<U>, Job> && !std::is_same_v<std::decay_t<U>, JobBuilder>)
		{
			static_assert(false, "Enqueue only supports objects of type Job");
		}

		else
		{
			EnqueueInternal(std::forward<JobBuilder>(static_cast<JobBuilder&&>(InJob)));

			// #NOTE: Safeguarding the notify can destroy performance in high enqueue situations. This leaves a blind spot potential,
			// but the risk is worth it. Even if a blind spot signal happens, the worker will just sleep until a new enqueue arrives, where it can recover.
			//QueueCV.Lock();
			QueueCV.NotifyOne();  // Notify one sleeper. They will work steal if they don't get the job enqueued directly.
			//QueueCV.Unlock();
		}
	}

	template <size_t Size>
	void Manager::Enqueue(Job (&InJobs)[Size])
	{
		for (auto Iter = 0; Iter < Size; ++Iter)
		{
			EnqueueInternal(InJobs[Iter]);
		}

		QueueCV.NotifyAll();  // Notify all sleepers.
	}

	template <typename U>
	void Manager::Enqueue(U&& InJob, const std::shared_ptr<Counter<>>& InCounter)
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
			InCounter->operator++();
			InJob.AtomicCounter = InCounter;

			Enqueue(InJob);
		}
	}

	template <size_t Size>
	void Manager::Enqueue(Job (&InJobs)[Size], const std::shared_ptr<Counter<>>& InCounter)
	{
		InCounter->operator+=(Size);

		for (auto Iter = 0; Iter < Size; ++Iter)
		{
			InJobs[Iter].AtomicCounter = InCounter;
		}

		Enqueue(InJobs);
	}

	template <typename U>
	std::shared_ptr<Counter<>> Manager::Enqueue(U&& InJob, const std::string& Group)
	{
		if constexpr (!std::is_same_v<std::decay_t<U>, Job> && !std::is_same_v<std::decay_t<U>, JobBuilder>)
		{
			static_assert(false, "Enqueue only supports objects of type Job");
		}

		else
		{
			std::shared_ptr<Counter<>> GroupCounter;

			auto AllocateCounter{ []() { return std::make_shared<Counter<>>(1); } };

			if (Group.empty())
			{
				GroupCounter = std::move(AllocateCounter());
			}

			else
			{
				auto GroupIter{ GroupMap.find(Group) };

				if (GroupIter != GroupMap.end())
				{
					GroupCounter = GroupIter->second;
					GroupCounter->operator++();
				}

				else
				{
					GroupCounter = std::move(AllocateCounter());
					GroupMap.insert({ Group, GroupCounter });
				}
			}

			InJob.AtomicCounter = GroupCounter;
			Enqueue(InJob);

			return GroupCounter;
		}
	}

	template <size_t Size>
	std::shared_ptr<Counter<>> Manager::Enqueue(Job (&InJobs)[Size], const std::string& Group)
	{
		std::shared_ptr<Counter<>> GroupCounter;

		auto AllocateCounter{ []() { return std::make_shared<Counter<>>(1); } };

		if (Group.empty())
		{
			GroupCounter = std::move(AllocateCounter());
		}

		else
		{
			auto GroupIter{ GroupMap.find(Group) };

			if (GroupIter != GroupMap.end())
			{
				GroupCounter = GroupIter->second;
				GroupCounter->operator+=(Size);
			}

			else
			{
				GroupCounter = std::move(AllocateCounter());
				GroupMap.insert({ Group, GroupCounter });
			}
		}

		for (auto Iter = 0; Iter < Size; ++Iter)
		{
			InJobs[Iter].AtomicCounter = GroupCounter;
		}

		Enqueue(InJobs);

		return GroupCounter;
	}

	bool Manager::IsValidID(size_t ID) const
	{
		return ID != InvalidID;
	}

	bool Manager::CanContinue() const
	{
		return !Shutdown.load(std::memory_order_acquire);
	}
}