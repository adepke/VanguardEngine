// Copyright (c) 2019 Andrew Depke

#pragma once

#include <Jobs/Job.h>

namespace Jobs
{
	class JobBuilder : public Job
	{
		friend class Manager;

	private:
		using TreeType = std::vector<std::pair<std::vector<Job>, std::shared_ptr<Counter<>>>>;
		TreeType* JobTree = nullptr;  // We need a heap pointer so that the counter resources can be managed beyond the lifetime of this object, which will be thrown away once we execute.

		// Disabled indefinitely (for usage outside of the job system's internals). Did not find a clean solution for having the cleanup job signal
		// this object that the resource has been destroyed. Attempting to get the counter without such synchronization is unsafe since the cleanup
		// job will kill the memory we're waiting on. Previous attempts include wrapping JobTree in a shared pointer, which doesn't work since we can't
		// get a copy of that strong pointer over to the job, rebuilding a shared pointer would duplicate the reference counter. Also tried using a
		// custom data wrapper with an atomic flag, but ran into the same problem.
		Counter<>& GetCounter() const
		{
			return *std::prev(JobTree->end())->second;  // Return the last counter.
		}

	public:
		JobBuilder() = default;
		JobBuilder(Job::EntryType InEntry, void* InData = nullptr) : Job(InEntry, InData) { Stream = true; JobTree = new TreeType{}; }
		JobBuilder(const JobBuilder&) = default;
		// Do not delete JobTree in the destructor, that would tie us to a stack lifetime.

		// Since we rely on slicing, we need to be careful about swapping garbage.
		JobBuilder(JobBuilder&& Other) noexcept : Job(Other)
		{
			if (Other.Stream)
			{
				JobTree = std::move(Other.JobTree);
			}
		}

		JobBuilder& operator=(JobBuilder&& Other) noexcept
		{
			Job::operator=(std::forward<Job>(static_cast<Job&&>(Other)));

			if (Other.Stream)
			{
				JobTree = std::move(Other.JobTree);
			}

			return *this;
		}
		
		template <typename... T>
		JobBuilder& Then(T&&... Next)
		{
			static_assert((std::is_same_v<std::decay_t<decltype(Next)>, Job> && ...), "Job building can only append jobs in Then()");

			JobTree->push_back(std::make_pair(std::vector{ Next... }, std::make_shared<Counter<>>()));  // #TODO: Heap pool.

			return *this;
		}

		// Disabled indefinitely. See GetCounter().
		/*
		void Wait(Counter<>::Type ExpectedValue)
		{
			if (auto ThisCounter{ std::move(GetCounter()) }; ThisCounter)
			{
				ThisCounter->Wait(ExpectedValue);
			}
		}

		template <typename Rep, typename Period>
		void WaitFor(Counter<>::Type ExpectedValue, const std::chrono::duration<Rep, Period>& Timeout)
		{
			if (auto ThisCounter{ std::move(GetCounter()) }; ThisCounter)
			{
				ThisCounter->WaitFor(ExpectedValue, Timeout);
			}
		}
		*/

		void operator()(Manager* InOwner);
	};

	inline JobBuilder MakeJob(Job::EntryType InEntry, void* InData = nullptr)
	{
		return JobBuilder{ InEntry, InData };
	}
}