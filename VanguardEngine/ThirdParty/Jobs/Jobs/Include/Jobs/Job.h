// Copyright (c) 2019 Andrew Depke

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
		friend void ManagerFiberEntry(void* Data);

	public:
		using EntryType = void(*)(void* Data);
		EntryType Entry = nullptr;

	protected:
		bool Stream = false;  // Bit to determine if we're a stream structure (JobBuilder).

		void* Data = nullptr;
		std::weak_ptr<Counter<>> AtomicCounter;

		// List of dependencies this job needs before executing. Pairs of counters to expected values.
		using DependencyType = std::pair<std::weak_ptr<Counter<>>, Counter<>::Type>;
		// MSVC throws errors with the stack allocator in debugging mode, so just turn it off unless we're in an optimized build.
#if _DEBUG || NDEBUG
		std::vector<DependencyType> Dependencies;
#else
		std::vector<DependencyType, DependencyAllocator<DependencyType, 2>> Dependencies;
#endif

	public:
		Job() = default;
		Job(EntryType InEntry, void* InData = nullptr) : Entry(InEntry), Data(InData) {}

		void AddDependency(const std::shared_ptr<Counter<>>& Handle, const Counter<>::Type ExpectedValue = Counter<>::Type{ 0 })
		{
			Dependencies.push_back({ Handle, ExpectedValue });
		}

		void operator()()
		{
			JOBS_ASSERT(Entry, "Attempted to execute empty job.");

			return Entry(Data);
		}
	};
}