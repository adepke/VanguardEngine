// Copyright (c) 2019-2021 Andrew Depke

#include <Jobs/Worker.h>

#include <Jobs/Manager.h>
#include <Jobs/Logging.h>
#include <Jobs/Assert.h>
#include <Jobs/Fiber.h>

#if JOBS_PLATFORM_WINDOWS
  #include <Jobs/WindowsMinimal.h>
#endif
#if JOBS_PLATFORM_POSIX
  #include <pthread.h>
#endif

namespace Jobs
{
	Worker::Worker(Manager* inOwner, size_t inID, EntryType entry) : owner(inOwner), id(inID)
	{
		JOBS_SCOPED_STAT("Worker Creation");

		JOBS_LOG(LogLevel::Log, "Building thread.");

		JOBS_ASSERT(inOwner, "Worker constructor needs a valid owner.");

		Fiber baseFiber;  // Holds the real thread fiber.
		threadFiber = new Fiber{ Manager::fiberStackSize, entry, owner };

		threadHandle = std::thread{ [this, &baseFiber]()
		{
			threadFiber->Schedule(baseFiber);  // Schedule our fiber from a new thread. We will never resume.
		} };

#if JOBS_PLATFORM_WINDOWS
		SetThreadAffinityMask(threadHandle.native_handle(), static_cast<size_t>(1) << inID);
		SetThreadDescription(threadHandle.native_handle(), L"Jobs Worker");
#else
		cpu_set_t cpuSet;
		CPU_ZERO(&cpuSet);
		CPU_SET(inID, &cpuSet);

		const auto result = pthread_setaffinity_np(threadHandle.native_handle(), sizeof(cpuSet), &cpuSet)
		pthread_setname_np(threadHandle.native_handle(), "Jobs Worker");

		JOBS_ASSERT(result == 0, "Error occurred in pthread_setaffinity_np().");
#endif
	}

	Worker::~Worker()
	{
		if (threadHandle.native_handle())
		{
			// Only log if we're not a moved worker shell.
			JOBS_LOG(LogLevel::Log, "Destroying thread.");
		}

		// The thread may have already finished, so validate our handle first.
		if (threadHandle.joinable())
		{
			threadHandle.join();
		}

		delete threadFiber;
	}

	void Worker::Swap(Worker& other) noexcept
	{
		std::swap(owner, other.owner);
		std::swap(threadHandle, other.threadHandle);
		std::swap(id, other.id);
		std::swap(threadFiber, other.threadFiber);
		std::swap(jobQueue, other.jobQueue);
	}
}