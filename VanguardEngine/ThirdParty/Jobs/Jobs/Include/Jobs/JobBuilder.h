// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Jobs/Job.h>

namespace Jobs
{
	class JobBuilder : public Job
	{
		friend class Manager;

	private:
		using TreeType = std::vector<std::pair<std::vector<Job>, std::shared_ptr<Counter<>>>>;
		TreeType* jobTree = nullptr;  // We need a heap pointer so that the counter resources can be managed beyond the lifetime of this object, which will be thrown away once we execute.

		// Disabled indefinitely (for usage outside of the job system's internals). Did not find a clean solution for having the cleanup job signal
		// this object that the resource has been destroyed. Attempting to get the counter without such synchronization is unsafe since the cleanup
		// job will kill the memory we're waiting on. Previous attempts include wrapping JobTree in a shared pointer, which doesn't work since we can't
		// get a copy of that strong pointer over to the job, rebuilding a shared pointer would duplicate the reference counter. Also tried using a
		// custom data wrapper with an atomic flag, but ran into the same problem.
		Counter<>& GetCounter() const
		{
			return *std::prev(jobTree->end())->second;  // Return the last counter.
		}

	public:
		JobBuilder() = default;
		JobBuilder(Job::EntryType entry, void* data = nullptr) : Job(entry, data) { stream = true; jobTree = new TreeType{}; }
		JobBuilder(const JobBuilder&) = default;
		// Do not delete JobTree in the destructor, that would tie us to a stack lifetime.

		// Since we rely on slicing, we need to be careful about swapping garbage.
		JobBuilder(JobBuilder&& other) noexcept : Job(other)
		{
			if (other.stream)
			{
				jobTree = std::move(other.jobTree);
			}
		}

		JobBuilder& operator=(JobBuilder&& other) noexcept
		{
			Job::operator=(std::forward<Job>(static_cast<Job&&>(other)));

			if (other.stream)
			{
				jobTree = std::move(other.jobTree);
			}

			return *this;
		}
		
		template <typename... T>
		JobBuilder& Then(T&&... next)
		{
			static_assert((std::is_same_v<std::decay_t<decltype(next)>, Job> && ...), "Job building can only append jobs in Then()");

			jobTree->push_back(std::make_pair(std::vector{ next... }, std::make_shared<Counter<>>()));  // #TODO: Heap pool.

			return *this;
		}

		// Disabled indefinitely. See GetCounter().
		/*
		void Wait(Counter<>::Type expectedValue)
		{
			if (auto thisCounter{ std::move(GetCounter()) }; thisCounter)
			{
				thisCounter->Wait(expectedValue);
			}
		}

		template <typename Rep, typename Period>
		void WaitFor(Counter<>::Type expectedValue, const std::chrono::duration<Rep, Period>& timeout)
		{
			if (auto thisCounter{ std::move(GetCounter()) }; thisCounter)
			{
				thisCounter->WaitFor(expectedValue, timeout);
			}
		}
		*/

		void operator()(Manager* owner);
	};

	inline JobBuilder MakeJob(Job::EntryType entry, void* data = nullptr)
	{
		return JobBuilder{ entry, data };
	}
}