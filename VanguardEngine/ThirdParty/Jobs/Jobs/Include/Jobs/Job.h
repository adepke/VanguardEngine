// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Jobs/Counter.h>
#include <Jobs/DependencyAllocator.h>
#include <Jobs/Assert.h>

#include <memory>  // std::shared_ptr, std::weak_ptr
#include <vector>  // std::vector
#include <utility>  // std::pair

namespace Jobs
{
	class Job
	{
		friend class Manager;
		friend void ManagerFiberEntry(void*);

	public:
		using EntryType = void(*)(Manager*, void*);
		EntryType entry = nullptr;

	protected:
		bool stream = false;  // Bit to determine if we're a stream structure (JobBuilder).

		void* data = nullptr;
		std::weak_ptr<Counter<>> atomicCounter;

		// List of dependencies this job needs before executing. Pairs of counters to expected values.
		using DependencyType = std::pair<std::weak_ptr<Counter<>>, Counter<>::Type>;
		// MSVC throws errors with the stack allocator in debugging mode, so just turn it off unless we're in an optimized build.
#if !NDEBUG
		std::vector<DependencyType> dependencies;
#else
		std::vector<DependencyType, DependencyAllocator<DependencyType, 2>> dependencies;
#endif

	public:
		Job() = default;
		Job(EntryType inEntry, void* inData = nullptr) : entry(inEntry), data(inData) {}

		void AddDependency(const std::shared_ptr<Counter<>>& handle, const Counter<>::Type expectedValue = Counter<>::Type{ 0 })
		{
			dependencies.push_back({ handle, expectedValue });
		}

		void operator()(Manager* owner)
		{
			JOBS_ASSERT(entry, "Attempted to execute empty job.");

			return entry(owner, data);
		}
	};
}